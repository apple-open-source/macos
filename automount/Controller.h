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

- (unsigned int)automount:(Vnode *)n directory:(String *)dir args:(int)mntargs;

- (BOOL)createPath:(String *)path;
- (BOOL)createPath:(String *)path withUid:(int)uid;

- (Vnode *)vnodeWithID:(unsigned int)n;
- (void)registerVnode:(Vnode *)v;
- (void)destroyVnode:(Vnode *)v;

- (Map *)rootMap;

- (unsigned int)autoMap:(Map *)map name:(String *)name directory:(String *)dir;
- (unsigned int)mountmap:(String *)mapname directory:(String *)dir;
- (unsigned int)nfsmount:(Vnode *)v withUid:(int)uid;

- (Server *)serverWithName:(String *)name;

- (void)timeout;
- (void)unmountAutomounts:(int)use_force;
- (void)reInit;
- (unsigned int)attemptUnmount:(Vnode *)v;
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

#ifndef __APPLE__
- (void)mtabUpdate:(Vnode *)v;
#endif

@end

#endif __CONTROLLER_H__
