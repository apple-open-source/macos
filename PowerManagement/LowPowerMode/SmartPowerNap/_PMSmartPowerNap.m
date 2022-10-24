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
//  _PMSmartPowerNap.m
//  _PMSmartPowerNap
//
//  Created by Archana on 9/14/21.
//

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <libproc.h>
#import <os/log.h>
#import <os/transaction_private.h>
#import <notify.h>
#import <IOKit/pwr_mgt/IOPMLibPrivate.h>
#import "_PMSmartPowerNap.h"

static os_log_t smartpowernap_log = NULL;
#define LOG_STREAM  smartpowernap_log

#define ERROR_LOG(fmt, args...) \
{  \
    if (smartpowernap_log) \
    os_log_error(LOG_STREAM, fmt, ##args); \
    else\
    os_log_error(OS_LOG_DEFAULT, fmt, ##args); \
}

#define INFO_LOG(fmt, args...) \
{  \
    if (smartpowernap_log) \
    os_log(LOG_STREAM, fmt, ##args); \
    else \
    os_log(OS_LOG_DEFAULT, fmt, ##args); \
}

#define FAULT_LOG(fmt, args...) \
{  \
    os_log_fault(LOG_STREAM, fmt, ##args); \
}

NSString *const kPMSmartPowerNapServiceName = @"com.apple.powerd.smartpowernap";


@implementation _PMSmartPowerNap

- (instancetype)init {
    self = [super init];
    //setup logging
    smartpowernap_log = os_log_create("com.apple.powerd", "smartpowernap");

    // establish connection
    if (self) {
        _connection = [[NSXPCConnection alloc]initWithMachServiceName:kPMSmartPowerNapServiceName options:NSXPCConnectionPrivileged];
        _connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(_PMSmartPowerNapProtocol)];
        _connection.exportedObject = self;
        _connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(_PMSmartPowerNapCallbackProtocol)];

        // interruption handler
        __weak typeof(self) welf = self;
        [_connection setInterruptionHandler:^{
            typeof(self) client = welf;
            if (!client) {
                return;
            }
            INFO_LOG("Connection to powerd interrupted");
            client.connection_interrupted = YES;
        }];
        [_connection resume];
        _connection_interrupted = NO;
        INFO_LOG("Initialized connection");

        // Register to re-establish connection on powerd's restart
        static int resync_token;
        int status = notify_register_dispatch(kIOUserAssertionReSync, &resync_token, dispatch_get_main_queue(), ^(int token __unused) {
            typeof(self) client = welf;
            if (!client) {
                return;
            }
            if (client.connection_interrupted) {
                INFO_LOG("Powerd has restarted");
                [client reRegister];
                client.connection_interrupted = NO;
            }
        });
        if (status != NOTIFY_STATUS_OK) {
            ERROR_LOG("Failed to register for reconnect with powerd 0x%x", status);
        }
    }
    return self;
}

- (void)registerWithCallback:(dispatch_queue_t)queue callback:(_PMSmartPowerNapCallback)callback {

    pid_t my_pid = getpid();
    char name[128];
    proc_name(my_pid, name, sizeof(name));
    NSString *client_name = [NSString stringWithFormat:@"%s", name];
    _identifier = client_name;
    [self registerWithIdentifier:_identifier];
    _callback_queue = queue;
    _callback = callback;
}

- (_PMSmartPowerNapState)state {
    return _current_state;
}

- (void)registerWithIdentifier:(NSString *)identifier {
    if (identifier) {
        INFO_LOG("registerWithIdentifier %@", identifier);
        [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
                ERROR_LOG("Failed to register %@", error);
        }] registerWithIdentifier:identifier];
    } else {
        FAULT_LOG("Failed to register. Expected non-null identifier");
    }
}

- (void)reRegister {
    INFO_LOG("re-register with powerd with identifier: %@", _identifier);
    [self registerWithIdentifier:_identifier];
}

- (void)unregister {
    if (_identifier) {
        [self unregisterWithIdentifier:_identifier];
    } else {
        ERROR_LOG("unregister called without registering");
        FAULT_LOG("unregister called without registering. No identifier found");
    }
}

- (void)unregisterWithIdentifier:(NSString *)identifier {
    INFO_LOG("unregisterWithIdentifier %@", identifier);
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
                ERROR_LOG("Failed to unregister %@", error);
    }]unregisterWithIdentifier:identifier];
}

- (void)updateState:(_PMSmartPowerNapState)state {
    // callback on queue supplied
    _current_state = state;
    NS_VALID_UNTIL_END_OF_SCOPE os_transaction_t transaction = os_transaction_create("com.apple.powerd.smartpowernap.callback");
    if (_callback && _callback_queue) {
        dispatch_async(_callback_queue, ^{
            (void)transaction;
            self->_callback(state);
        });
    }
}

- (void)setState:(_PMSmartPowerNapState)state {
    INFO_LOG("Updating state to %d", (int)state);
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
                ERROR_LOG("Failed to update state %@", error);
    }]setState:state];
}

- (void)setSPNReentryCoolOffPeriod:(uint32_t)seconds {
    INFO_LOG("Updating SPN Re-entry cool off period");
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
                ERROR_LOG("Failed to update SPN re-entry cooloff period %@", error);
    }]setSPNReentryCoolOffPeriod:seconds];
}

- (void)setSPNReentryDelaySeconds:(uint32_t)seconds {
    INFO_LOG("Updating SPN Re-entry delay");
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            ERROR_LOG("Failed to SPN Re-entry delay %@", error);
    }]setSPNReentryDelaySeconds:seconds];
}

- (void)setSPNRequeryDelta:(uint32_t)seconds {
    INFO_LOG("Updating SPN Re-query delay");
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            ERROR_LOG("Failed to SPN Re-query delay %@", error);
    }]setSPNRequeryDelta:seconds];
}

- (void)setSPNMotionAlarmThreshold:(uint32_t)seconds {
    INFO_LOG("Updating SPN motion alarm threshold");
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            ERROR_LOG("Failed to SPN Re-entry delay %@", error);
    }]setSPNMotionAlarmThreshold:seconds];
}

- (void)setSPNMotionAlarmStartThreshold:(uint32_t)seconds {
    INFO_LOG("Updating SPN motion alarm start threshold");
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
            ERROR_LOG("Failed to SPN Re-entry delay %@", error);
    }]setSPNMotionAlarmStartThreshold:seconds];
}

- (void) syncStateWithHandler:(_PMSmartPowerNapCallback)handler {
    [[_connection remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        ERROR_LOG("syncStateWithHandler failed %@", error);
    }] syncStateWithHandler:handler];
}

- (_PMSmartPowerNapState)syncState {
    __block _PMSmartPowerNapState new_state = _PMSmartPowerNapStateOff;
    [[_connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        ERROR_LOG("syncState synchronous update failed %@", error);
    }]syncStateWithHandler:^(_PMSmartPowerNapState state) {
        new_state = state;
        self.current_state = new_state;
    }];
    return new_state;
}
@end

