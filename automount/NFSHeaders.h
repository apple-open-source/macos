/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _NFSHEADERS_H_
#define _NFSHEADERS_H_

#import <sys/types.h>
#import <sys/time.h>
#import <netinet/in.h>
#import <sys/param.h>
#import <sys/ucred.h>

#define _KERNEL
#import <sys/mount.h>
#undef _KERNEL

#import <rpc/types.h>
#import <rpc/rpc.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/svc.h>

#ifdef __APPLE__
#import <nfs/rpcv2.h>

#define NFS_PROGRAM
#import <nfs/nfsproto.h>
#undef NFS_PROGRAM

#define _KERNEL
#import <nfs/nfs.h>
#undef _KERNEL
#endif

#import <nfs_prot.h>

#ifndef __APPLE__
#define NFSCLIENT
#import <nfs/nfs_mount.h>
#define NFS_WSIZE 8192
#define NFS_RSIZE 8192
#define MNT_RDONLY M_RDONLY
#define MNT_NOSUID M_NOSUID
#define MNT_NOEXEC 0
#define MNT_NODEV 0
#define MNT_UNION 0
#define MNT_SYNCHRONOUS 0
#define NFSMNT_NOCONN 0
#define NFSMNT_NFSV3 0
#define NFSMNT_KERB 0
#define NFSMNT_DUMBTIMR 0
#define NFSMNT_RESVPORT 0
#define NFSMNT_RDIRPLUS 0

#endif
#endif _NFSHEADERS_H_