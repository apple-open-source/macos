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

#import "TPHash.h"
#import "TPSigningKey.h"
#import "TPDecrypter.h"
#import "TPTypes.h"

@class TPCircle;
@class TPPeerPermanentInfo;
@class TPPeerStableInfo;
@class TPPeerDynamicInfo;
@class TPVoucher;
@class TPPolicyDocument;


typedef NS_OPTIONS(NSUInteger, TPPeerStatus) {
    // Set if at least one of the peers I trust trusts me.
    TPPeerStatusPartiallyReciprocated   = 1 << 0,
    
    // Set if all of the peers I trust trust me.
    TPPeerStatusFullyReciprocated       = 1 << 1,
    
    // Set if I have been kicked out of trust.
    TPPeerStatusExcluded                = 1 << 2,
    
    // Set if my epoch is behind the latest epoch.
    TPPeerStatusOutdatedEpoch           = 1 << 3,
    
    // Set if my epoch is two or more epochs behind the latest.
    TPPeerStatusAncientEpoch            = 1 << 4,
};


NS_ASSUME_NONNULL_BEGIN

/*!
 TPModel implements the Octagon Trust model, as per
 https://confluence.sd.apple.com/display/KEY/Octagon+Trust

 It maintains a collection of peers and a collection of circles,
 to track the peers and circles in CloudKit.
 (This class does not communicate with CloudKit. The client of this class does that.)
 
 Normally there would be just one instance of TPModel, associated with a particular Apple ID.
 (This class doesn't need to know what the Apple ID is.)

 This interface does not expose TPPeer* because the caller might mutate the peer object.
 All the objects exposed by this interface are immutable.
*/
@interface TPModel : NSObject

- (instancetype)initWithDecrypter:(id<TPDecrypter>)decrypter;

- (TPCounter)latestEpochAmongPeerIDs:(NSSet<NSString*> *)peerIDs;

- (void)registerPolicyDocument:(TPPolicyDocument *)policyDoc;

/*!
 Register a peer with the given permanentInfo.
 
 To access this peer invoke other TPModel methods and
 pass permanentInfo.peerID as the peerID argument.
 
 (If a peer with this permanentInfo is already registered then registering it again
 does nothing, and the existing TPPeer object internal to TPModel retains its state.)
 */
- (void)registerPeerWithPermanentInfo:(TPPeerPermanentInfo *)permanentInfo;

- (void)deletePeerWithID:(NSString *)peerID;

- (BOOL)hasPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (TPPeerStatus)statusOfPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (TPPeerPermanentInfo *)getPermanentInfoForPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (nullable TPPeerStableInfo *)getStableInfoForPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (nullable NSData *)getWrappedPrivateKeysForPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (void)setWrappedPrivateKeys:(nullable NSData *)wrappedPrivateKeys
                forPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (nullable TPPeerDynamicInfo *)getDynamicInfoForPeerWithID:(NSString *)peerID;

/*!
 Asserts that peerID is registered.
 */
- (nullable TPCircle *)getCircleForPeerWithID:(NSString *)peerID;


- (void)registerCircle:(TPCircle *)circle;

- (void)deleteCircleWithID:(NSString *)circleID;

/*!
 Returns nil if no circle matching circleID is registered.
 */
- (nullable TPCircle *)circleWithID:(NSString *)circleID;


/*!
 An "update" with unchanged data is considered success.
 Asserts that peerID is registered.
 */
- (TPResult)updateStableInfo:(TPPeerStableInfo *)stableInfo
               forPeerWithID:(NSString *)peerID;

/*!
 Returns nil with error if the peer's trustSigningKey is unable to create
 a signature because the private key is unavailable, e.g. because the device is locked.
 
 Asserts peerID is registered.
 */
- (TPPeerStableInfo *)createStableInfoWithDictionary:(NSDictionary *)dict
                                       policyVersion:(TPCounter)policyVersion
                                          policyHash:(NSString *)policyHash
                                       policySecrets:(nullable NSDictionary *)policySecrets
                                       forPeerWithID:(NSString *)peerID
                                               error:(NSError **)error;


/*!
 An "update" with unchanged data is considered success.
 Asserts peerID is registered.
 */
- (TPResult)updateDynamicInfo:(TPPeerDynamicInfo *)dynamicInfo
                forPeerWithID:(NSString *)peerID;


/*!
 The returned voucher is not registered.
 Asserts sponsorID is registered.

 Returns nil with nil error if policy determines that the sponsor
 is not permitted to introduce this candidate.
 
 Returns nil with error if the sponsor's trustSigningKey is unable to create
 a signature because the private key is unavailable, e.g. because the device is locked.
 
 The candidate need not be registered before making this call.
 */
- (nullable TPVoucher *)createVoucherForCandidate:(TPPeerPermanentInfo *)candidate
                                    withSponsorID:(NSString *)sponsorID
                                            error:(NSError **)error;

/*!
 Asserts that the sponsor is registered, so that the signature check can be performed.
 The beneficiary need not be registered.
 */
- (TPResult)registerVoucher:(TPVoucher *)voucher;


- (NSSet<NSString*> *)calculateUnusedCircleIDs;

/*!
 Calculates updated dynamic info for a given peer,
 according to the membership convergence algorithm.
 
 This method does not update the model. The calculated circle is not registered
 and the peer is not updated with the calculated dynamicInfo. It is the caller's
 responsibility to register/update them once they have been persisted to CloudKit.
 
 Peers listed in addingPeerIDs are taken to have been explicitly trusted by the user.
 When the user adds a member of addingPeerIDs into trust, the peers already trusted
 by that new peer are also taken to be trusted, even if the new peer is not qualified by
 policy to *introduce* them into trust. This is neccessary in a scenario where a mid-level
 device approves a lowly device, and the new lowly device should trust the high-level devices
 already in the circle. The mid-level device is not *introducing* the high-level devices.
 
 Peers listed in removingPeerIDs are excluded from trust.

 Returns nil with error if the peer's trustSigningKey is unable to create
 a signature because the private key is unavailable, e.g. because the device is locked.
 
 Asserts peerID is registered.
 */
- (nullable TPPeerDynamicInfo *)calculateDynamicInfoForPeerWithID:(NSString *)peerID
                                                    addingPeerIDs:(nullable NSArray<NSString*> *)addingPeerIDs
                                                  removingPeerIDs:(nullable NSArray<NSString*> *)removingPeerIDs
                                                     createClique:(nullable NSString* (^)())createClique
                                                    updatedCircle:(TPCircle * _Nullable * _Nullable)updatedCircle
                                                            error:(NSError **)error;

/*!
 A convenience method for tests, this calls calculateDynamicInfoForPeerWithID,
 registers the results and returns the new circle.
 */
- (TPCircle *)advancePeerWithID:(NSString *)peerID
                  addingPeerIDs:(nullable NSArray<NSString*> *)addingPeerIDs
                removingPeerIDs:(nullable NSArray<NSString*> *)removingPeerIDs
                   createClique:(nullable NSString* (^)())createClique;

/*!
 From our trusted peers, return the subset that is allowed
 to access the given view, according to the current policy.
 
 Asserts peerID is registered.
 */
- (nullable NSSet<NSString*> *)getPeerIDsTrustedByPeerWithID:(NSString *)peerID
                                                toAccessView:(NSString *)view
                                                       error:(NSError **)error;

/*!
 Returns a dictionary mapping from each peer ID
 to the most recent clock seen from that peer.
 */
- (NSDictionary<NSString*,NSNumber*> *)vectorClock;

@end

NS_ASSUME_NONNULL_END
