/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2023 Apple Computer, Inc.  All Rights Reserved.
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

#include <dlfcn.h>
#include "IOHIDGamePolicySupport.h"
#include <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#define GamePolicyFrameworkPath "/System/Library/PrivateFrameworks/GamePolicy.framework/GamePolicy"

#define GPMonitorClassName "GPProcessMonitor"

@class GPProcessInfo;
@class GPProcessMonitor;

@protocol GPMonitorInfoProtocol <NSObject>

typedef void(^GPProcessInfoUpdateHandler)(GPProcessMonitor *monitor, GPProcessInfo *processInfo);

- (void)setUpdateHandler:(nullable GPProcessInfoUpdateHandler)block;

@end

static void *__loadFramework()
{
 
    static void *gpHandle = NULL;
    static dispatch_once_t gpOnce = 0;

    dispatch_once(&gpOnce, ^{
        gpHandle = dlopen(GamePolicyFrameworkPath, RTLD_LAZY);

        if (!gpHandle) {
            return;
        }
    });

    return gpHandle;
}

IOReturn IOHIDAnalyticsGetConsoleModeStatus(ConsoleModeBlock replyBlock)
{

    IOReturn returnStatus = kIOReturnError;

    void * gpHandle = __loadFramework();

    if (!gpHandle) {
        return kIOReturnError;
    }

    Class GPMonitor_class = NSClassFromString(@GPMonitorClassName);
    if (GPMonitor_class) {
        SEL montiorForCurrentProcess_sel = sel_getUid("monitorForCurrentProcess");
        id<GPMonitorInfoProtocol> gameMonitor = [GPMonitor_class performSelector:montiorForCurrentProcess_sel];
        if (!gameMonitor) {
            return kIOReturnError;
        }

        [gameMonitor setUpdateHandler:^(GPProcessMonitor * __unused monitor, GPProcessInfo *processInfo) {
            id processInfoID = processInfo;
            SEL isIdentifiedGame_sel = sel_getUid("isIdentifiedGame");
            BOOL nameStatus = (BOOL)[processInfoID performSelector:isIdentifiedGame_sel];
            replyBlock(nameStatus == YES);
        }];
        returnStatus = kIOReturnSuccess;
    }

    return returnStatus;
}
