/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef __VNODE_H__
#define __VNODE_H__
#import "automount.h"
#import "RRObject.h"
#import "Array.h"
#import "NFSHeaders.h"

#define URL_KEY_STRING "url=="
#define AUTH_URL_KEY_STRING "authenticated_url=="

@class Server;
@class String;
@class Map;

/*
 * File handles in V3 are 64 bytes, V2 uses 32.
 * This structure must be the size of the largest file handle.
 */
#define FHSIZE3 64
struct file_handle
{
	unsigned int node_id;
	char zero[FHSIZE3 - sizeof(unsigned int)];
};

@interface Vnode : RRObject
{
	String *relpath;
	String *fullpath;
	String *name;
	String *src;
	String *link;
	Server *server;
	String *vfsType;
	String *urlString;
	String *authenticated_urlString;
	BOOL mountInProgress;
	BOOL mounted;
	BOOL mountPathCreated;
	BOOL fake;
	Vnode *supernode;
	Map *map;
	Array *dirlist;
	Array *subnodes;
	Array *submounts;
	struct fattr attributes;
	int mntArgs;
	struct nfs_args nfsArgs;
	unsigned int mountTime;
	unsigned int timeToLive;
	int mntTimeout;
	unsigned int forcedNFSVersion;
	unsigned int forcedProtocol;
	unsigned int nfsStatus;
	struct MountProgressRecord mountInfo;
	unsigned long transactionID;
	BOOL marked;
}

- (String *)relativepath;
- (String *)path;

- (String *)name;
- (void)setName:(String *)n;

- (String *)link;
- (void)setLink:(String *)l;

- (String *)source;
- (void)setSource:(String *)s;

- (String *)vfsType;
- (void)setVfsType:(String *)s;

- (Server *)server;
- (void)setServer:(Server *)s;

- (void)setUrlString:(String *)n;
- (String *)urlString;
/* debugURLString avoids dynamically creating a URL */
- (String *)debugURLString;

- (Map *)map;
- (void)setMap:(Map *)m;

- (struct fattr)attributes;
- (void)setAttributes:(struct fattr)a;

- (void)resetTime;
- (void)markDirectoryChanged;
- (void)resetAllTimes;
- (void)resetMountTime;

- (ftype)type;
- (void)setType:(ftype)t;

- (unsigned int)mode;
- (void)setMode:(unsigned int)m;

- (unsigned int)nodeID;
- (void)setNodeID:(unsigned int)n;

- (void)setupOptions:(Array *)o;

- (struct nfs_args)nfsArgs;
- (unsigned int)forcedNFSVersion;
- (unsigned int)forcedProtocol;

- (int)mntArgs;
- (void)addMntArg:(int)arg;
- (int)mntTimeout;

- (BOOL)mounted;
- (void)setMounted:(BOOL)m;

- (BOOL)mountInProgress;
- (void)setMountInProgress:(BOOL)newMountInProgressState;

- (BOOL)fakeMount;
- (void)setFakeMount:(BOOL)m;

- (unsigned int)mountTime;
- (unsigned int)mountTimeToLive;

- (BOOL)mountPathCreated;
- (void)setMountPathCreated:(BOOL)m;

- (unsigned long)transactionID;
- (void)setTransactionID:(unsigned long)xid;

- (unsigned int)nfsStatus;
- (void)setNfsStatus:(unsigned int)s;

- (struct MountProgressRecord *)mountInfo;

- (void)getFileHandle:(nfs_fh *)fh;

- (Vnode *)lookup:(String *)name;

- (int)symlinkWithName:(char *)from to:(char *)to attributes:(struct nfsv2_sattr *)attributes;
- (int)remove:(String *)name;

- (Vnode *)parent;
- (void)setParent:(Vnode *)p;

- (Array *)children;
- (void)addChild:(Vnode *)child;
- (void)removeChild:(Vnode *)child;
- (BOOL)hasChildren;

- (Array *)dirlist;

- (BOOL)needsAuthentication;
- (BOOL)serverMounted;
- (BOOL)descendantMounted;

- (BOOL)marked;
- (void)setMarked:(BOOL)m;

- (BOOL)checkForUnmount;

@end

#endif __VNODE_H__
