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

#if OCTAGON
#import <Foundation/Foundation.h>
#import "OTLocalStore.h"
#import "OTCloudStore.h"
#import "OTEscrowKeys.h"
#import "OTIdentity.h"
#import "OTBottledPeer.h"
#import "OTBottledPeerSigned.h"
#import "OTRamping.h"
#import "OTDefines.h"
#import "OTPreflightInfo.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OTContextIdentityProvider <NSObject>
- (nullable OTIdentity *) currentIdentity:(NSError**) error;
@end


@interface OTContext : NSObject

@property (nonatomic, readonly) NSString* contextID;
@property (nonatomic, readonly) NSString* dsid;
@property (nonatomic, readonly) OTCloudStore* cloudStore;

@property (nonatomic, readonly) CKKSLockStateTracker* lockStateTracker;
@property (nonatomic, readonly) CKKSCKAccountStateTracker* accountTracker;
@property (nonatomic, readonly) CKKSReachabilityTracker *reachabilityTracker;

- (nullable instancetype) initWithContextID:(NSString*)contextID
                                       dsid:(NSString*)dsid
                                 localStore:(OTLocalStore*)localStore
                                 cloudStore:(nullable OTCloudStore*)cloudStore
                           identityProvider:(id <OTContextIdentityProvider>)identityProvider
                                      error:(NSError**)error;

- (nullable OTBottledPeerSigned *) restoreFromEscrowRecordID:(NSString*)escrowRecordID
                                             secret:(NSData*)secret
                                              error:(NSError**)error;

- (NSData* _Nullable) makeMeSomeEntropy:(int)requiredLength;
- (nullable OTPreflightInfo*) preflightBottledPeer:(NSString*)contextID
                                           entropy:(NSData*)entropy
                                             error:(NSError**)error;
- (BOOL)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   error:(NSError**)error;

-(OctagonBottleCheckState)doesThisDeviceHaveABottle:(NSError**)error;

@end
NS_ASSUME_NONNULL_END
#endif

