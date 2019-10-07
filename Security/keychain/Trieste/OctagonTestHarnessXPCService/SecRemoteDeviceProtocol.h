/*
 * Copyright (c) 2017 - 2018 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>
#import <Security/SecureObjectSync/SOSTypes.h>

NS_ASSUME_NONNULL_BEGIN

@class KCPairingChannelContext;

@protocol DevicePairingProtocol <NSObject>
- (void)exchangePacket:(NSData *_Nullable)data complete:(void (^)(bool complete, NSData *_Nullable result, NSError *_Nullable error))complete;
- (void)validateStart:(void(^)(bool result, NSError * _Nullable error))complete;
@end

@protocol SecRemoteDeviceProtocol <NSObject>

// Local Keychain
- (void)secItemAdd:(NSDictionary *)input complete:(void (^)(OSStatus, NSDictionary * _Nullable))reply;
- (void)secItemCopyMatching:(NSDictionary *)input complete:(void (^)(OSStatus, NSArray<NSDictionary *>* _Nullable))replyreply;

// SOS trust
- (void)setUserCredentials:(NSString *)username password:(NSString *)password complete:(void (^)(bool success, NSError *error))complete;
- (void)setupSOSCircle:(NSString *)username password:(NSString *)password complete:(void (^)(bool success,  NSError *_Nullable error))complete;
- (void)sosCircleStatus:(void(^)(SOSCCStatus status, NSError *_Nullable error))complete;
- (void)sosCircleStatusNonCached:(void(^)(SOSCCStatus status, NSError *_Nullable error))complete;
- (void)sosViewStatus:(NSString *) view withCompletion: (void(^)(SOSViewResultCode status, NSError * _Nullable error))complete;
- (void)sosICKStatus: (void(^)(bool status))complete;
- (void)sosCachedViewBitmask: (void(^)(uint64_t bitmask))complete;
- (void)sosPeerID:(void(^)(NSString * _Nullable peerID))complete;
- (void)sosPeerSerial:(void(^)(NSString * _Nullable peerSerial))complete;
- (void)sosCirclePeerIDs:(void(^)(NSArray<NSString *> * _Nullable peerIDs))complete;
- (void)sosRequestToJoin:(void(^)(bool success, NSString *peerID, NSError * _Nullable error))complete;
- (void)sosLeaveCircle: (void(^)(bool success, NSError * _Nullable error))complete;
- (void)sosApprovePeer:(NSString *)peerID complete:(void(^)(BOOL success, NSError * _Nullable error))complete;
- (void)sosGhostBust:(SOSAccountGhostBustingOptions)options complete:(void(^)(bool busted, NSError *error))complete;
- (void)sosCircleHash: (void(^)(NSString *data, NSError * _Nullable error))complete;


// SOS syncing
- (void)sosWaitForInitialSync:(void(^)(bool success, NSError * _Nullable error))complete;
- (void)sosEnableAllViews:(void(^)(BOOL success, NSError * _Nullable error))complete;

// IdMS interface
- (void)deviceInfo:(void (^)(NSString *_Nullable mid, NSString *_Nullable serial, NSError * _Nullable error))complete;

// Pairing
- (void)pairingChannelSetup:(bool)initiator pairingContext:(KCPairingChannelContext * _Nullable)context complete:(void (^)(id<DevicePairingProtocol> _Nullable, NSError * _Nullable error))complete;

// Diagnostics
- (void)diagnosticsLeaks:(void(^)(bool success, NSString *_Nullable outout, NSError * _Nullable error))complete;
- (void)diagnosticsCPUUsage:(void(^)(bool success, uint64_t user_usec, uint64_t sys_usec, NSError *_Nullable error))complete;
- (void)diagnosticsDiskUsage:(void(^)(bool success, uint64_t usage, NSError * _Nullable error))complete;

// CKKS
- (void)selfPeersForView:(NSString *)view complete:(void (^)(NSArray<NSDictionary *> *result, NSError *error))complete;

// Octagon
- (void)otReset:(NSString *)altDSID complete:(void (^)(bool success,  NSError *_Nullable error))complete;

- (void)otPeerID:(NSString *)altDSID complete:(void (^)(NSString *peerID,  NSError *_Nullable error))complete;
- (void)otInCircle:(NSString *)altDSID complete:(void (^)(bool inCircle,  NSError *_Nullable error))complete;

@end

NS_ASSUME_NONNULL_END
