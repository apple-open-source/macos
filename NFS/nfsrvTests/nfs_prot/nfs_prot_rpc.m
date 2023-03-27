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

#import <XCTest/XCTest.h>

#include "nfs_prot_rpc.h"

void
doNullRPC(CLIENT *clnt)
{
    void *arg = NULL;
    void *result = NULL;

    result = nfsproc3_null_3((void*)&arg, clnt);
    if (result == NULL) {
        XCTFail("nfsproc_null_3 returned null");
    }
}

GETATTR3res *
doGetattrRPC(CLIENT *clnt, nfs_fh3 *object)
{
    GETATTR3args args = { .object = *object};
    GETATTR3res *result = NULL;

    result = nfsproc3_getattr_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_getattr_3 returned null");
    }

    return result;
}

SETATTR3res *
doSetattrRPC(CLIENT *clnt, nfs_fh3 *object, sattr3 *new_attributes, struct timespec *guard)
{
    SETATTR3args args = {};
    SETATTR3res *result = NULL;

    args.object = *object;
    args.new_attributes = *new_attributes;
    if (guard) {
        args.guard.check = YES;
        args.guard.sattrguard3_u.obj_ctime.seconds = (uint32_t)guard->tv_sec;
        args.guard.sattrguard3_u.obj_ctime.nseconds = (uint32_t)guard->tv_nsec;
    }

    result = nfsproc3_setattr_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_setattr_3 returned null");
    }

    return result;
}

LOOKUP3res *
doLookupRPC(CLIENT *clnt, nfs_fh3 *dir, char *name)
{
    LOOKUP3args args = { .what.dir = *dir, .what.name = name};
    LOOKUP3res *result = NULL;

    result = nfsproc3_lookup_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_lookup_3 returned null");
    }

    return result;
}

ACCESS3res *
doAccessRPC(CLIENT *clnt, nfs_fh3 *object, uint32_t access)
{
    ACCESS3args args = { .object = *object, .access = access };
    ACCESS3res *result = NULL;

    result = nfsproc3_access_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_access_3 returned null");
    }

    return result;
}

READLINK3res *
doReadlinkRPC(CLIENT *clnt, nfs_fh3 *symlink)
{
    READLINK3args args = { .symlink = *symlink };
    READLINK3res *result = NULL;

    result = nfsproc3_readlink_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_readlink_3 returned null");
    }

    return result;
}

READ3res *
doReadRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count)
{
    READ3args args = { .file = *file, .offset = offset, .count = count };
    READ3res *result = NULL;

    result = nfsproc3_read_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_read_3 returned null");
    }

    return result;
}

WRITE3res *
doWriteRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count, stable_how stable, u_int data_len, char *data_val)
{
    WRITE3args args = { .file = *file, .offset = offset, .count = count, .stable = stable, .data.data_len = data_len, .data.data_val = data_val };
    WRITE3res *result = NULL;

    result = nfsproc3_write_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_write_3 returned null");
    }

    return result;
}

CREATE3res *
doCreateRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, struct createhow3 *how)
{
    CREATE3args args = { .where.dir = *dir, .where.name = name, .how = *how };
    CREATE3res *result = NULL;

    result = nfsproc3_create_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_create_3 returned null");
    }

    return result;
}

MKDIR3res *
doMkdirRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, sattr3 *attributes)
{
    MKDIR3args args = { .where.dir = *dir, .where.name = name, .attributes = *attributes };
    MKDIR3res *result = NULL;

    result = nfsproc3_mkdir_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_mkdir_3 returned null");
    }

    return result;
}

SYMLINK3res *
doSymlinkRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, sattr3 *symlink_attributes, nfspath3 symlink_data)
{
    SYMLINK3args args = { .where.dir = *dir, .where.name = name, .symlink.symlink_attributes = *symlink_attributes, .symlink.symlink_data = symlink_data };
    SYMLINK3res *result = NULL;

    result = nfsproc3_symlink_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_symlink_3 returned null");
    }

    return result;
}

MKNOD3res *
doMknodRPC(CLIENT *clnt, nfs_fh3 *where_dir, char *where_name, struct mknoddata3 *what)
{
    MKNOD3args args = { .where.dir = *where_dir, .where.name = where_name, .what = *what };
    MKNOD3res *result = NULL;

    result = nfsproc3_mknod_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_mknod_3 returned null");
    }

    return result;
}

REMOVE3res *
doRemoveRPC(CLIENT *clnt, nfs_fh3 *dir, char *name)
{
    REMOVE3args args = { .object.dir = *dir, .object.name = name };
    REMOVE3res *result = NULL;

    result = nfsproc3_remove_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_remove_3 returned null");
    }

    return result;
}

RMDIR3res *
doRMDirRPC(CLIENT *clnt, nfs_fh3 *dir, char *name)
{
    RMDIR3args args = { .object.dir = *dir, .object.name = name };
    RMDIR3res *result = NULL;

    result = nfsproc3_rmdir_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_rmdir_3 returned null");
    }

    return result;
}

RENAME3res *
doRenameRPC(CLIENT *clnt, nfs_fh3 *from_dir, char *from_name, nfs_fh3 *to_dir, char *to_name)
{
    RENAME3args args = { .from.dir = *from_dir, .from.name = from_name, .to.dir = *to_dir, .to.name = to_name };
    RENAME3res *result = NULL;

    result = nfsproc3_rename_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_rename_3 returned null");
    }

    return result;
}

LINK3res *
doLinkRPC(CLIENT *clnt, nfs_fh3 *file, nfs_fh3 *link_dir, char *link_name)
{
    LINK3args args = { .file = *file, .link.dir = *link_dir, .link.name = link_name };
    LINK3res *result = NULL;

    result = nfsproc3_link_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_link_3 returned null");
    }

    return result;
}

READDIR3res *
doReaddirRPC(CLIENT *clnt, nfs_fh3 *dir, cookie3 cookie, cookieverf3 *cookieverf, count3 count)
{
    READDIR3args args = { .dir = *dir, .cookie = cookie, .count = count };
    READDIR3res *result = NULL;
    memcpy(args.cookieverf, *cookieverf, sizeof(args.cookieverf));

    result = nfsproc3_readdir_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_readdir_3 returned null");
    }

    return result;
}

READDIRPLUS3res *
doReaddirplusRPC(CLIENT *clnt, nfs_fh3 *dir, cookie3 cookie, cookieverf3 *cookieverf, count3 dircount, count3 maxcount)
{
    READDIRPLUS3args args = { .dir = *dir, .cookie = cookie, .dircount = dircount, .maxcount = maxcount };
    READDIRPLUS3res *result = NULL;
    memcpy(args.cookieverf, *cookieverf, sizeof(args.cookieverf));

    result = nfsproc3_readdirplus_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_readdirplus_3 returned null");
    }

    return result;
}

FSSTAT3res *
doFSStatRPC(CLIENT *clnt, nfs_fh3 *fsroot)
{
    FSSTAT3args args = { .fsroot = *fsroot };
    FSSTAT3res *result = NULL;

    result = nfsproc3_fsstat_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_fsstat_3 returned null");
    }

    return result;
}

FSINFO3res *
doFSinfoRPC(CLIENT *clnt, nfs_fh3 *fsroot)
{
    FSINFO3args args = { .fsroot = *fsroot };
    FSINFO3res *result = NULL;

    result = nfsproc3_fsinfo_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_fsinfo_3 returned null");
    }

    return result;
}

PATHCONF3res *
doPathconfRPC(CLIENT *clnt, nfs_fh3 *object)
{
    PATHCONF3args args = { .object = *object };
    PATHCONF3res *result = NULL;

    result = nfsproc3_pathconf_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_pathconf_3 returned null");
    }

    return result;
}

COMMIT3res *
doCommitRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count)
{
    COMMIT3args args = { .file = *file, .offset = offset, .count = count };
    COMMIT3res *result = NULL;

    result = nfsproc3_commit_3((void*)&args, clnt);
    if (result == NULL) {
        XCTFail("nfsproc3_commit_3 returned null");
    }

    return result;
}
