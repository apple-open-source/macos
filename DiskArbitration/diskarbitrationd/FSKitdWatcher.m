/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#import <CoreFoundation/CoreFoundation.h>
#import "DADisk.h"
#import "DAFileSystem.h"
#import <Foundation/Foundation.h>
#import <os/feature_private.h>
#import "DASupport.h"       // Get DA_FSKIT

#ifdef DA_FSKIT
#import <FSKit/FSKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface FSKitdWatcher : NSObject

+(instancetype)newWithClient:(FSClient *)client;

@end

NS_ASSUME_NONNULL_END

#import "FSKitdWatcher.h"
#import "DADisk.h"
#import "DAMain.h"
#import "DAQueue.h"
#import "DAServer.h"

@interface FSKitdWatcher ()

@property (retain)  FSClient    *client;

-(void)handleDiskUpdateWithoutProbe:(FSTaskDescription *)taskDesc;
-(void)handleDiskUpdateAndProbe:(FSTaskDescription *)taskDesc;

-(instancetype)setupNotificationHandler;
-(void)handleTaskUpdate:(FSTaskDescription *)taskDesc;

@end

@implementation FSKitdWatcher

-(instancetype)initWithClient:(FSClient *)client
{
    if (!client) {
        return nil;
    }

    self = [super init];
    if (self) {
        _client = client;
        self = [self setupNotificationHandler];
    }
    return self;
}

-(instancetype)init
{
    return [self initWithClient:[FSClient new]];
}

+(instancetype)newWithClient:(FSClient *)client
{
    FSKitdWatcher *rv = [[self alloc] initWithClient:client];
    return rv;
}

-(void)dealloc
{
    _client = nil;
}

-(instancetype)setupNotificationHandler
{
    FSKitdWatcher       *rv = self;
    __block NSError     *err;
    dispatch_group_t     g = dispatch_group_create();
    dispatch_group_enter(g);

    FSTaskUpdateHandlerBlock_t handler;
    /*
     * Following block (stored in `handler`) is the code we run each time
     * fskitd tells us about a task update. We will get FSTaskRunning, FSTaskPaused,
     * and FSTaskFinished messages. We hand them off to [self handleTaskUpdate]
     * on the DAServerWorkLoop() queue
     *
     *  This block is invoked on the serial receive queue for the fskitd->client
     * side of our NSXPC connection.
     */
    handler = ^(FSTaskDescription * _Nullable td,
                NSError * _Nullable error) {
        os_log_info(fskit_std_log(), "FSTaskUpdateHandler run w td %@ and error %@",
               td.debugDescription, error);
        if (error) {
            // Oops, FSClient died.
            _client = [FSClient new];
            [self setupNotificationHandler];
        } else {
            // Hop the work over to the server work loop
            dispatch_async(DAServerWorkLoop(), ^{
                [self handleTaskUpdate:td];
            });
        }
    };

    [_client setTaskUpdateHandler:handler
                            reply:^(NSError * _Nullable error) {
        err = error;
        dispatch_group_leave(g);
    }];

    dispatch_group_wait(g, DISPATCH_TIME_FOREVER);

    if (err) {
        // Didn't register handler, => we can't watch fskitd
        rv = nil;
    }

    return rv;
}

#pragma mark - Update handlers, scheduled to run on DAServerWorkLoop()

-(DADiskRef)diskForTask:(FSTaskDescription *)taskDesc
{
    FSResource              *res = taskDesc.taskResource;
    FSBlockDeviceResource   *disk = [FSBlockDeviceResource dynamicCast:res];
    NSString                *devPath;
    DADiskRef                daDisk = NULL;

    if (!disk) {
        // Our resource isn't an FSBlockDeviceResource
        return NULL;
    }
    devPath = [NSString stringWithFormat:@"/dev/%@", disk.bsdName];
    daDisk = DADiskListGetDisk(devPath.UTF8String);
    return daDisk;
}

/*
 * handleDiskUpdateAndProbe - a format operation finished. See what the device is now
 */
-(void)handleDiskUpdateAndProbe:(FSTaskDescription *)taskDesc
{
    DADiskRef   disk;

    disk = [self diskForTask:taskDesc];
    os_log_info(fskit_std_log(), "%s got disk %@, format finished so reprobe it",
                __FUNCTION__, disk);
    if (disk) {
        DADiskProbe(disk, NULL);
    }
}

/*
 * handleDiskUpdateWithoutProbe - a task entered Running or ended
 *
 *      Other than formats ending, we get here.
 */
-(void)handleDiskUpdateWithoutProbe:(FSTaskDescription *)taskDesc
{
    // Do a DADiskChange
    DADiskRef   disk;

    disk = [self diskForTask:taskDesc];
    if (disk) {
        //
    }
}

-(void)handleTaskUpdate:(FSTaskDescription *)taskDesc
{
    if (taskDesc.taskState == FSTaskPaused) {
        return; // No updates for paused tasks
    } else if (taskDesc.taskState == FSTaskFinished
               && [taskDesc.taskPurpose isEqualToString:FSTaskPurposeFormat]) {
        // format just ended
        [self handleDiskUpdateAndProbe:taskDesc];
    } else {
        [self handleDiskUpdateWithoutProbe:taskDesc];
    }
}

@end

static  FSKitdWatcher   *gWatcher;

#pragma mark - external C routines

void
DAStartFSKitdWatcher(void)
{
    if ([FSKitConstants class] == NULL) {
        // No FSKit. Don't touch anything FSKit-related
        gFSKitMissing = true;
        os_log(OS_LOG_DEFAULT, "FSKit unavailable, skipping FSKit watcher");
        return;
    }
    if ( ! os_feature_enabled(DiskArbitration, enableFSKitModules) ) {
        os_log(fskit_std_log(), "Skipping FSKitdWatcher as pref is off");
        // If the preference is off, don't set the watcher
        return;
    }

    // We don't have a global client, so just make a watcher
    gWatcher = [FSKitdWatcher new];
    if (!gWatcher) {
        // Oops!
        os_log_error(fskit_std_log(), "DAStartFSKitdWatcher unable to setup watcher");
    }
    os_log_info(fskit_std_log(), "FSKitdWatcher setup done");
}

#endif /* DA_FSKIT */
