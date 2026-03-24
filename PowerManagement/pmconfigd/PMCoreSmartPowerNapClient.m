/*
* Copyright (c) 2022 Apple Computer, Inc. All rights reserved.
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
//
//  PMCoreSmartPowerNapClient.m
//  powerd-binary powerd-binary-Embedded
//
//  Created by Prateek Malhotra on 12/7/22.
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import "PMCoreSmartPowerNapClient.h"
#import <os/log.h>

static os_log_t cspnClientLog;

@interface PMCoreSmartPowerNapClient()

@property (nonatomic, readwrite) NSXPCConnection *connection;

@end

@implementation PMCoreSmartPowerNapClient

- (instancetype)initWithConnection: (NSXPCConnection *)conn {

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cspnClientLog = os_log_create("com.apple.powerd", "coreSmartPowerNap");
    });

    self = [super init];
    if (self) {
        _connection = conn;
        __weak typeof(self) welf = self;
        _connection.interruptionHandler = ^{
            typeof(self) client = welf;
            if (!client) {
                return;
            }
            os_log_info(cspnClientLog, "Connection to client interrupted");
            [client.connection invalidate];
            if (client.onInterruption) {
                client.onInterruption();
            }
            client.connection = nil;
        };
        _connection.invalidationHandler = ^{
            typeof(self) client = welf;
            if (!client) {
                return;
            }
            os_log_info(cspnClientLog, "Connection to client invalidated");
            if (client.onInterruption) {
                client.onInterruption();
            }
            client.connection = nil;
        };
    }
    return self;
}

- (void)dealloc
{
    [_connection invalidate];
    _connection = nil;
}
@end
