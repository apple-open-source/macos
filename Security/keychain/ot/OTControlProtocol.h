/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
NS_ASSUME_NONNULL_BEGIN

@protocol OTControlProtocol <NSObject>
- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID reply:(void (^)(NSData* _Nullable signingKeyData, NSData* _Nullable encryptionKeyData, NSError * _Nullable error))reply;
- (void)octagonEncryptionPublicKey:(void (^)(NSData* _Nullable encryptionKey, NSError * _Nullable))reply;;
- (void)octagonSigningPublicKey:(void (^)(NSData* _Nullable signingKey, NSError * _Nullable))reply;;
- (void)listOfEligibleBottledPeerRecords:(void (^)(NSArray* listOfRecords, NSError *))reply;
- (void)signOut:(void (^)(BOOL result, NSError * _Nullable signedOutError))reply;
- (void)signIn:(NSString*)dsid reply:(void (^)(BOOL result, NSError * _Nullable signedInError))reply;
- (void)reset:(void (^)(BOOL result, NSError * _Nullable error))reply;
- (void)scheduleCFUForFuture;

- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply;
- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable error))reply;
- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable error))reply;
@end

NSXPCInterface* OTSetupControlProtocol(NSXPCInterface* interface);

NS_ASSUME_NONNULL_END
