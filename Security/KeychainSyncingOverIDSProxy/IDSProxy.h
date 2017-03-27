/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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

typedef enum {
    kIDSStartPingTestMessage = 1,
    kIDSEndPingTestMessage= 2,
    kIDSSendOneMessage = 3,
    kIDSPeerReceivedACK = 4,
    kIDSPeerAvailability = 6,
    kIDSPeerAvailabilityDone = 7,
    kIDSKeychainSyncIDSFragmentation = 8,
} idsOperation;

typedef enum {
    kSecIDSErrorNoDeviceID = -1, //default case
    kSecIDSErrorNotRegistered = -2,
    kSecIDSErrorFailedToSend=-3,
    kSecIDSErrorCouldNotFindMatchingAuthToken = -4,
    kSecIDSErrorDeviceIsLocked = -5,
    kSecIDSErrorNoPeersAvailable = -6
    
} idsError;


@interface KeychainSyncingOverIDSProxy  :  NSObject <IDSServiceDelegate>
{
    IDSService      *_service;
    NSString        *deviceID;
    NSMutableDictionary *deviceIDFromAuthToken;
}

@property (retain, nonatomic) NSMutableDictionary *deviceIDFromAuthToken;
@property (retain, nonatomic) NSString *deviceID;
@property (retain, nonatomic) NSMutableDictionary *unhandledMessageBuffer;
@property (retain, nonatomic) NSMutableDictionary *shadowPendingMessages;
@property (retain, nonatomic) NSMutableDictionary *allFragmentedMessages;
@property (retain, nonatomic) NSMutableDictionary *pingTimers;
@property (retain, nonatomic) NSMutableDictionary *messagesInFlight;
@property (retain, nonatomic) NSMutableDictionary *peerNextSendCache; //dictioanry of device ID -> time stamp of when to send next

@property (retain, nonatomic) NSArray* listOfDevices;

@property (atomic) dispatch_source_t penaltyTimer;
@property (atomic) bool penaltyTimerScheduled;
@property (retain, atomic) NSMutableDictionary *monitor;
@property (retain, atomic) NSDictionary *queuedMessages;

@property (atomic) bool isIDSInitDone;
@property (atomic) bool shadowDoInitializeIDSService;
@property (atomic) bool isSecDRunningAsRoot;
@property (atomic) bool doesSecDHavePeer;
@property (atomic) dispatch_queue_t calloutQueue;
@property (atomic) bool isLocked;
@property (atomic) bool unlockedSinceBoot;
@property (atomic) dispatch_source_t retryTimer;
@property (atomic) bool retryTimerScheduled;
@property (atomic) bool inCallout;
@property (atomic) bool setIDSDeviceID;
@property (atomic) bool shadowDoSetIDSDeviceID;

@property (atomic) bool handleAllPendingMessages;
@property (atomic) bool shadowHandleAllPendingMessages;
@property (atomic) bool sendRestoredMessages;

+ (KeychainSyncingOverIDSProxy *) idsProxy;

- (id)init;

- (void) importIDSState: (NSMutableDictionary*) state;

- (void) doSetIDSDeviceID;
- (void) doIDSInitialization;
- (void) calloutWith: (void(^)(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSettingDeviceID))) callout;
- (void) sendKeysCallout: (NSMutableDictionary *(^)(NSMutableDictionary* pending, NSError** error)) handleMessages;
- (void)persistState;
- (void) sendPersistedMessagesAgain;
- (NSDictionary*) retrievePendingMessages;

- (void)scheduleRetryRequestTimer;
@end

NSString* createErrorString(NSString* format, ...);
