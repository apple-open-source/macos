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
#ifndef __AUTOMOUNT_H__
#define __AUTOMOUNT_H__

#import <sys/types.h>
#import <objc/Object.h>
#import <sys/queue.h>

@class Vnode;
@class NSLMap;
@class Controller;

extern Controller *controller;
extern id dot;
extern id dotdot;

extern unsigned int GlobalMountTimeout;
extern unsigned int GlobalTimeToLive;

extern int debug;
extern int debug_proc;
extern int debug_mount;
extern int debug_select;
extern int debug_options;
extern int debug_nsl;

extern int osType;

extern int gWakeupFDs[2];

extern BOOL gTerminating;

#define REQ_SHUTDOWN 'S'
#define REQ_REINIT 'R'
#define REQ_NETWORKCHANGE 'N'
#define REQ_USERLOGOUT 'L'
#define REQ_PROCESS_RESULTS 'P'
#define REQ_ALARM 'A'
#define REQ_UNMOUNT 'U'
#define REQ_MOUNTCOMPLETE 'M'
#define REQ_USR2 '2'

struct MountProgressRecord {
	LIST_ENTRY(MountProgressRecord) mpr_link;
	pid_t mpr_mountpid;
	Vnode *mpr_vp;
};
typedef LIST_HEAD(MountProgressRecord_List, MountProgressRecord) MountProgressRecord_List;

extern MountProgressRecord_List gMountsInProgress;

extern BOOL gForkedMountInProgress;
extern BOOL gForkedMount;
extern BOOL gBlockedMountDependency;
extern unsigned long gBlockingMountTransactionID;
extern int gMountResult;

struct NSLMapListEntry {
	LIST_ENTRY(NSLMapListEntry) mle_link;
	NSLMap *mle_map;
};
typedef LIST_HEAD(NSLMapList, NSLMapListEntry) NSLMapList;

extern NSLMapList gNSLMapList;

#endif __AUTOMOUNT_H__
