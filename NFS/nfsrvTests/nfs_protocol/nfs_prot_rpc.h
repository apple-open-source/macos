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

#ifndef NFS_PROT_RPC_H
#define NFS_PROT_RPC_H

#include "nfs_prot.h"

/* =============== NFSv2 =============== */

void doNullRPC_v2(CLIENT *clnt);
attrstat *doGetattrRPC_v2(CLIENT *clnt, nfs_fh *object);
attrstat *doSetattrRPC_v2(CLIENT *clnt, nfs_fh *file, sattr *attributes);
diropres *doLookupRPC_v2(CLIENT *clnt, nfs_fh *dir, filename name);
readlinkres *doReadlinkRPC_v2(CLIENT *clnt, nfs_fh *symlink);
readres *doReadRPC_v2(CLIENT *clnt, nfs_fh *file, u_int offset, u_int count);
attrstat *doWriteRPC_v2(CLIENT *clnt, nfs_fh *file, u_int offset, u_int data_len, char *data_val);
diropres *doCreateRPC_v2(CLIENT *clnt, nfs_fh *dir, char *name, sattr *attributes);
nfsstat *doRemoveRPC_v2(CLIENT *clnt, nfs_fh *dir, char *name);
nfsstat *doRenameRPC_v2(CLIENT *clnt, nfs_fh *from_dir, char *from_name, nfs_fh *to_dir, char *to_name);
nfsstat *doLinkRPC_v2(CLIENT *clnt, nfs_fh *file, nfs_fh *link_dir, char *link_name);
nfsstat *doSymlinkRPC_v2(CLIENT *clnt, nfs_fh *dir, char *name, nfspath to, sattr *attributes);
diropres *doMkdirRPC_v2(CLIENT *clnt, nfs_fh *dir, char *name, sattr *attributes);
nfsstat *doRMDirRPC_v2(CLIENT *clnt, nfs_fh *dir, char *name);
readdirres *doReaddirRPC_v2(CLIENT *clnt, nfs_fh *dir, nfscookie *cookie, u_int count);
statfsres *doStatfsRPC_v2(CLIENT *clnt, nfs_fh *fsroot);

/* =============== NFSv3 =============== */

void doNullRPC(CLIENT *clnt);
GETATTR3res *doGetattrRPC(CLIENT *clnt, nfs_fh3 *object);
SETATTR3res *doSetattrRPC(CLIENT *clnt, nfs_fh3 *object, sattr3 *new_attributes, struct timespec *guard);
LOOKUP3res *doLookupRPC(CLIENT *clnt, nfs_fh3 *dir, char *name);
ACCESS3res *doAccessRPC(CLIENT *clnt, nfs_fh3 *object, uint32_t access);
READLINK3res *doReadlinkRPC(CLIENT *clnt, nfs_fh3 *symlink);
READ3res *doReadRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count);
WRITE3res *doWriteRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count, stable_how stable, u_int data_len, char *data_val);
CREATE3res *doCreateRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, struct createhow3 *how);
MKDIR3res *doMkdirRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, sattr3 *attributes);
SYMLINK3res *doSymlinkRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, sattr3 *symlink_attributes, nfspath3 symlink_data);
MKNOD3res *doMknodRPC(CLIENT *clnt, nfs_fh3 *where_dir, char *where_name, struct mknoddata3 *what);
REMOVE3res *doRemoveRPC(CLIENT *clnt, nfs_fh3 *dir, char *name);
RMDIR3res *doRMDirRPC(CLIENT *clnt, nfs_fh3 *dir, char *name);
RENAME3res *doRenameRPC(CLIENT *clnt, nfs_fh3 *from_dir, char *from_name, nfs_fh3 *to_dir, char *to_name);
LINK3res *doLinkRPC(CLIENT *clnt, nfs_fh3 *file, nfs_fh3 *link_dir, char *link_name);
READDIR3res *doReaddirRPC(CLIENT *clnt, nfs_fh3 *dir, cookie3 cookie, cookieverf3 *cookieverf, count3 count);
READDIRPLUS3res *doReaddirplusRPC(CLIENT *clnt, nfs_fh3 *dir, cookie3 cookie, cookieverf3 *cookieverf, count3 dircount, count3 maxcount);
FSSTAT3res *doFSStatRPC(CLIENT *clnt, nfs_fh3 *fsroot);
FSINFO3res *doFSinfoRPC(CLIENT *clnt, nfs_fh3 *fsroot);
PATHCONF3res *doPathconfRPC(CLIENT *clnt, nfs_fh3 *object);
COMMIT3res *doCommitRPC(CLIENT *clnt, nfs_fh3 *file, offset3 offset, count3 count);

#endif /* NFS_PROT_RPC_H */
