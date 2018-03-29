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

// You must be 64-bit to use this class.
#if __OBJC2__

#import <Foundation/Foundation.h>
#import <Security/OTConstants.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTControl : NSObject
+ (OTControl* _Nullable)controlObject:(NSError* _Nullable __autoreleasing* _Nullable)error;
- (instancetype)initWithConnection:(NSXPCConnection*)connection;

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID
          reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error))reply;
- (void)encryptionKey:(void (^)(NSData* result, NSError* _Nullable error))reply;
- (void)signingKey:(void (^)(NSData* result, NSError* _Nullable error))reply;
- (void)listOfRecords:(void (^)(NSArray* list, NSError* _Nullable error))reply;
- (void)signOut:(void (^)(BOOL result, NSError * _Nullable error))reply;
- (void)signIn:(NSString*)dsid reply:(void (^)(BOOL result, NSError * _Nullable error))reply;
- (void)reset:(void (^)(BOOL result, NSError* _Nullable error))reply;

// Call this to 'preflight' a bottled peer entry. This will create sufficient entropy, derive and save all relevant keys,
// then return the entropy to the caller. If something goes wrong during this process, do not store the returned entropy.
- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply;

// Call this to 'launch' a preflighted bottled peer entry. This indicates that you've successfully stored the entropy,
// and we should save the bottled peer entry off-device for later retrieval.
- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable error))reply;

// Call this to scrub the launch of a preflighted bottled peer entry. This indicates you've terminally failed to store the
// preflighted entropy, and this bottled peer will never be used again and can be deleted.
- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable error))reply;

@end

NS_ASSUME_NONNULL_END
#endif  // __OBJC__
