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
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#import <CoreFoundation/CoreFoundation.h>

#import "RRObject.h"

#import <rpc/types.h>
#import <rpc/rpc.h>
#import <rpc/xdr.h>
#import <rpc/auth.h>
#import <rpc/clnt.h>
#import <rpc/svc.h>

@class Vnode;
@class Map;
@class Server;
@class String;
struct MountProgressRecord;
struct autofs_userreq;
struct fsid;

typedef struct
{
	unsigned int node_id;
	Vnode *node;
} node_table_entry;

typedef struct
{
	String *name;
	Server *server;
} server_table_entry;

typedef struct
{
	String *name;
	String *dir;
	String *mountdir;
	Map *map;
} map_table_entry;

@interface Controller : RRObject
{
	Map *rootMap;
	String *mountDirectory;
	node_table_entry *node_table;
	unsigned int node_table_count;
	server_table_entry *server_table;
	unsigned int server_table_count;
	map_table_entry *map_table;
	unsigned int map_table_count;
	unsigned int node_id;
	CFMutableDictionaryRef vnodeHashTable;
	SVCXPRT *transp;
	String *hostName;
	String *hostDNSDomain;
	String *hostArchitecture;
	String *hostByteOrder;
	String *hostOS;
	String *hostOSVersion;
	int hostOSVersionMajor;
	int hostOSVersionMinor;
	int afpLoaded;
}

- (Controller *)init:(char *)dir;

- (String *)mountDirectory;

- (int)autofsmount:(Vnode *)v directory:(String *)dir args:(int)mntargs;
- (int)automount:(Vnode *)n directory:(String *)dir args:(int)mntargs nfsmountoptions:(int)mntoptionflags;

- (BOOL)createPath:(String *)path withUid:(int)uid allowAnyExisting:(BOOL)allowAnyExisting;

- (void)hashVnode:(Vnode *)v;
- (void)unhashVnode:(Vnode *)v;
- (Vnode *)vnodeWithKey:(void *)vnodeKey;
- (void)registerVnode:(Vnode *)v;
- (BOOL)vnodeIsRegistered:(Vnode *)v;
- (Vnode *)vnodeWithID:(unsigned int)n;
- (void)compactVnodeTableFrom:(int)startIndex;
- (void)freeVnode:(Vnode *)v;
- (void)removeVnode:(Vnode *)v;
- (void)destroyVnode:(Vnode *)v;

- (Map *)rootMap;

- (int)autoMap:(Map *)map name:(String *)name directory:(String *)dir mountdirectory:(String *)mnt;
- (int)mountmap:(String *)mapname directory:(String *)dir mountdirectory:(String *)mnt;
- (int)nfsmount:(Vnode *)v withUid:(int)uid;
- (void)recordMountInProgressFor:(Vnode *)v uid:(uid_t)uid mountPID:(pid_t)mountPID transactionID:(u_long)transactionID;
- (BOOL)mountInProgressForVnode:(Vnode *)v forUID:(uid_t)uid;
- (BOOL)checkMountInProgressForTransaction:(u_long)transactionID;
- (void)completeMountInProgressBy:(pid_t)mountPID exitStatus:(int)exitStatus;

- (int)dispatch_autofsreq:(struct autofs_userreq *)req forFSID:(struct fsid *)fsid;

- (Server *)serverWithName:(String *)name;

- (void)timeout;
- (void)unmountAutomounts:(int)use_force;
- (void)validate;
- (void)reInit;
- (int)attemptUnmount:(Vnode *)v usingForce:(int)use_force;
- (void)checkForUnmounts;

- (void)printTree;
- (void)printNode:(Vnode *)v level:(unsigned int)l;

- (String *)hostName;
- (String *)hostDNSDomain;
- (String *)hostArchitecture;
- (String *)hostByteOrder;
- (String *)hostOS;
- (String *)hostOSVersion;
- (int)hostOSVersionMajor;
- (int)hostOSVersionMinor;

- (String *)findDirByMountDir:(String *)findmnt;

#ifndef __APPLE__
- (void)mtabUpdate:(Vnode *)v;
#endif

@end

#endif __CONTROLLER_H__
