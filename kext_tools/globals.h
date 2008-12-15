/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

// XX should be named kextd_globals.h or similar
// currently not suitable for sharing with other tools

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <IOKit/IOKitLib.h>
#include <libc.h>

#include <IOKit/kext/KXKextManager.h>
#include "request.h"
#include "PTLock.h"

#define kKXDiskArbMaxRetries          10

// in main.c
void kextd_handle_signal(int);
bool is_bootroot_active(void);

#define KEXTCACHE_DELAY_STD         (60)
#define KEXTCACHE_DELAY_FIRST_BOOT  (60 * 5)

extern Boolean g_first_boot;
extern Boolean g_safe_boot_mode;
extern char * g_kernel_file;
extern char * _kload_optimized_kern_sym_data;
extern uint32_t _kload_optimized_kern_sym_length;
extern char * g_patch_dir;
extern char * g_symbol_dir;
extern Boolean gOverwrite_symbols;

extern Boolean gStaleBootNotificationNeeded;

extern KXKextManagerRef gKextManager;
extern CFRunLoopRef gMainRunLoop;
extern CFRunLoopSourceRef gKernelRequestRunLoopSource;
extern CFRunLoopSourceRef gRescanRunLoopSource;
extern CFRunLoopSourceRef gCurrentNotificationRunLoopSource;

extern PTLockRef gKernelRequestQueueLock;
extern PTLockRef gKernSymfileDataLock;

extern queue_head_t g_request_queue;

// in request.c
extern CFMutableDictionaryRef gKextloadedKextPaths;

#ifndef NO_CFUserNotification

extern CFRunLoopSourceRef gNotificationQueueRunLoopSource;
extern CFMutableArrayRef gPendedNonsecureKextPaths; // alerts to be raised on user login
extern CFMutableDictionaryRef gNotifiedNonsecureKextPaths;
extern CFUserNotificationRef gCurrentNotification;

#endif /* NO_CFUserNotification */

// no-op if NO_CFUserNotification ...
void kextd_raise_notification(CFStringRef alertHeader, CFArrayRef messageArray);

extern uid_t logged_in_uid;

// in mig_server.c
extern uid_t gClientUID;

// in serialize_kextload.c
extern CFMachPortRef _kextload_lock;

#endif __GLOBALS_H__
