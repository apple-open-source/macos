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
#ifndef __MAP_H__
#define __MAP_H__

#import <sys/mount.h>

#import "RRObject.h"
#import "Array.h"
#import <mach/mach.h>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreServices/CoreServices.h>

@class Vnode;
@class String;

typedef enum AMMountStyle {
	kMountStyleUnknown = 0,
	kMountStyleParallel = 1,
	kMountStyleAutoFS = 2
} AMMountStyle;

@interface Map : RRObject
{
	String *mountPoint;
	AMMountStyle mountStyle;
	int NFSMountOptions;
	fsid_t mountedMapFSID;
	String *name;
	String *hostname;
	Vnode *root;
	CFMachPortContext *AMInfoPortContext;
	CFMachPortRef AMInfoServicePort;
	CFRunLoopSourceRef AMInforeqRLS;
}

- (id)init;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt mountedon:(String *)mnton;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt withRootVnodeClass:(Class)rootVnodeClass;
- (Map *)initWithParent:(Vnode *)p directory:(String *)dir from:(String *)ds mountdirectory:(String *)mnt mountedon:(String *)mnton withRootVnodeClass:(Class)rootVnodeClass;
- (void)cleanup;
- (unsigned int)didAutoMount;

- (void)setName:(String *)n;
- (String *)name;

- (void)setHostname:(String *)hn;
- (String *)hostname;

- (Vnode *)root;
- (Vnode *)lookupVnodePath:(String *)path from:(Vnode *)v;
- (BOOL)checkVnodePath:(String *)path from:(Vnode *)v;
- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v;
- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v withType:(String *)type;
- (Vnode *)mkdir:(String *)s attributes:(void *)x atVnode:(Vnode *)v;
- (Vnode *)symlink:(String *)l name:(String *)s atVnode:(Vnode *)v;

- (int)mount:(Vnode *)v withUid:(int)uid;
- (int)unmount:(Vnode *)v usingForce:(int)use_force withRemountOnFailure:(BOOL)remountOnFailure;
- (int)unmount:(Vnode *)v withRemountOnFailure:(BOOL)remountOnFailure;
- (int)unmountAutomounts:(int)use_force;
- (AMMountStyle)mountStyle;
- (void)setMountStyle:(AMMountStyle)style;
- (int)handle_autofsreq:(struct autofs_userreq *)req;
- (int)registerAMInfoService;
- (int)deregisterAMInfoService;
- (void)handleAMInfoRequest:(mach_msg_header_t *)msg ofSize:(size_t)size onPort:(mach_port_t)port;

- (void)setMountDirectory:(String *)mnt;
- (String *)mountPoint;
- (int)mountArgs;
- (void)setNFSMountOptions:(int)mountOptions;
- (int)NFSMountOptions;
- (void)setFSID:(fsid_t *)fsid;
- (fsid_t *)mountedFSID;
- (void)timeout;
- (void)reInit;
- (BOOL)acceptOptions:(Array *)opts;

- (String *)findTriggerPath:(Vnode *)curRoot findPath:(String *)findPath;

@end

#endif __MAP_H__
