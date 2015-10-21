/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
//  IDSProxy.h
//  ids-xpc

#import <Foundation/Foundation.h>
#import <dispatch/queue.h>
#import <xpc/xpc.h>
#import <IDS/IDS.h>
#import "SOSCloudKeychainClient.h"
#import <utilities/debugging.h>

#define IDSPROXYSCOPE "IDSProxy"
#define IDSServiceNameKeychainSync "com.apple.private.alloy.keychainsync"

typedef enum {
    kIDSStartPingTestMessage = 1,
    kIDSEndPingTestMessage= 2,
    kIDSSendOneMessage = 3,
    kIDSSyncMessagesRaw = 4,
    kIDSSyncMessagesCompact = 5,
    kIDSPeerAvailability = 6,
    kIDSPeerAvailabilityDone = 7
} idsOperation;

typedef enum {
    kSecIDSErrorNoDeviceID = -1, //default case
    kSecIDSErrorNotRegistered = -2,
    kSecIDSErrorFailedToSend=-3,
    kSecIDSErrorCouldNotFindMatchingAuthToken = -4,
    kSecIDSErrorDeviceIsLocked = -5,
    kSecIDSErrorBatchControllerUninitialized = -6
} idsError;


@interface IDSKeychainSyncingProxy  :  NSObject <IDSServiceDelegate, IDSBatchIDQueryControllerDelegate>
{
    CloudItemsChangedBlock itemsChangedCallback;
    IDSService      *_service;
    NSString        *_deviceID;
    NSMutableDictionary    *_unhandledMessageBuffer;
}

@property (retain, nonatomic) NSMutableDictionary *unhandledMessageBuffer;
@property (retain, nonatomic) NSMutableDictionary *shadowPendingMessages;

@property (atomic) bool isIDSInitDone;
@property (atomic) bool isSecDRunningAsRoot;
@property (atomic) dispatch_queue_t calloutQueue;
@property (atomic) bool isLocked;
@property (atomic) bool unlockedSinceBoot;
@property (atomic) dispatch_source_t syncTimer;
@property (atomic) bool syncTimerScheduled;
@property (atomic) dispatch_time_t deadline;
@property (atomic) dispatch_time_t lastSyncTime;
@property (atomic) bool inCallout;
@property (atomic) bool oldInCallout;
@property (atomic) bool setIDSDeviceID;
@property (atomic) bool shadowDoSetIDSDeviceID;

@property (atomic) bool handleAllPendingMessages;
@property (atomic) bool shadowHandleAllPendingMessages;

+ (IDSKeychainSyncingProxy *) idsProxy;

- (id)init;
- (void)setItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;
- (void)streamEvent:(xpc_object_t)notification;

- (BOOL) sendIDSMessage:(NSDictionary*)data name:(NSString*)deviceName peer:(NSString*) peerID error:(NSError**) error;
- (BOOL) doSetIDSDeviceID: (NSError**)error;
- (void) doIDSInitialization;
- (void) calloutWith: (void(^)(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSettingDeviceID))) callout;
- (void) sendKeysCallout: (NSMutableDictionary *(^)(NSMutableDictionary* pending, NSError** error)) handleMessages;

@end
