/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#if __OBJC2__

#ifndef OCTAGONTRUST_H
#define OCTAGONTRUST_H

#import <Foundation/Foundation.h>
#import <AppleFeatures/AppleFeatures.h>
#import <Security/OTClique.h>
#import <OctagonTrust/OTEscrowRecord.h>
#import <OctagonTrust/OTEscrowTranslation.h>
#import <OctagonTrust/OTEscrowAuthenticationInformation.h>
#import <OctagonTrust/OTICDPRecordContext.h>
#import <OctagonTrust/OTICDPRecordSilentContext.h>
#import <OctagonTrust/OTEscrowRecordMetadata.h>
#import <OctagonTrust/OTEscrowRecordMetadataClientMetadata.h>
#import <OctagonTrust/OTSecureElementPeerIdentity.h>
#import <OctagonTrust/OTCurrentSecureElementIdentities.h>
#import <OctagonTrust/OTNotifications.h>

NS_ASSUME_NONNULL_BEGIN

//! Project version number for OctagonTrust.
FOUNDATION_EXPORT double OctagonTrustVersionNumber;

//! Project version string for OctagonTrust.
FOUNDATION_EXPORT const unsigned char OctagonTrustVersionString[];

@interface OTConfigurationContext(Framework)
@property (nonatomic, copy, nullable) OTEscrowAuthenticationInformation* escrowAuth;
@end

@interface OTClique(Framework)

/* *
 * @abstract   Fetch recommended iCDP escrow records
 *
 * @param   data, context containing parameters to setup OTClique
 * @param  error, error gets filled if something goes horribly wrong
 *
 * @return  array of escrow records that can get a device back into trust
 */
+ (NSArray<OTEscrowRecord*>* _Nullable)fetchEscrowRecords:(OTConfigurationContext*)data error:(NSError**)error;


/* *
 * @abstract   Fetch all iCDP escrow records
 *
 * @param   data, context containing parameters to setup OTClique
 * @param  error, error gets filled if something goes horribly wrong
 *
 * @return  array of all escrow records (viable and legacy)
 */
+ (NSArray<OTEscrowRecord*>* _Nullable)fetchAllEscrowRecords:(OTConfigurationContext*)data error:(NSError**)error;

/* *
 * @abstract   Perform escrow recovery of a particular record (not silent)
 *
 * @param   data, context containing parameters to setup OTClique
 * @param   cdpContext, context containing parameters used in recovery
 * @param   escrowRecord, the chosen escrow record to recover from
 * @param  error, error gets filled if something goes horribly wrong
 *
 * @return  clique, returns a new clique instance
 */
+ (instancetype _Nullable)performEscrowRecovery:(OTConfigurationContext*)data
                                     cdpContext:(OTICDPRecordContext*)cdpContext
                                   escrowRecord:(OTEscrowRecord*)escrowRecord
                                          error:(NSError**)error;

/* *
 * @abstract   Perform a silent escrow recovery
 *
 * @param   data, context containing parameters to setup OTClique
 * @param   cdpContext, context containing parameters used in recovery
 * @param   allRecords, all fetched escrow records
 * @param  error, error gets filled if something goes horribly wrong
 * @return  clique, returns a new clique instance
 */
+ (instancetype _Nullable)performSilentEscrowRecovery:(OTConfigurationContext*)data
                                           cdpContext:(OTICDPRecordContext*)cdpContext
                                           allRecords:(NSArray<OTEscrowRecord*>*)allRecords
                                                error:(NSError**)error;

+ (BOOL) invalidateEscrowCache:(OTConfigurationContext*)data error:(NSError**)error;

/* *
 * @abstract Set the local SecureElement Identity for the given clique.
 *     This call can be done at any time, even if there is not valid or trusted clique present.
 *     When possible, Octagon will update the on-server state with this new identity.
 *     Your binary will need the entitlement 'com.apple.private.octagon.secureelement' set to YES.
 *
 * @param secureElementIdentity a peerID and SE identity blob-of-bytes
 * @param error An error parameter
 *
 * @return YES on success.
 */
- (BOOL)setLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                error:(NSError**)error;

/* *
 * @abstract Remove the local SecureElement Identity for the given clique.
 *     This call can be done at any time, even if there is not valid or trusted clique present.
 *     When possible, Octagon will update the on-server state with this new identity.
 *     Your binary will need the entitlement 'com.apple.private.octagon.secureelement' set to YES.
 *
 * @param sePeerID the peer ID to remove. If this is not the currently persisted local peer ID, this function will error.
 * @param error An error parameter
 *
 * @return YES on success.
 */
- (BOOL)removeLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                         error:(NSError**)error;

/* *
 * @abstract Fetches the current set of trusted SecureElementIdentities for the current clique.
 *     For the local peer's identity, this function returns what we believe the current value to be in the account,
 *     and will not take unwritten, pending changes from setLocalSecureElementIdentity/removeLocalSecureElementIdentityPeerID
 *     into account.
 *
 *     If the local device believes itself untrusted in Octagon, no identities will be returned.
 */
- (OTCurrentSecureElementIdentities* _Nullable)fetchTrustedSecureElementIdentities:(NSError**)error;


/* *
 * @abstract        Wait for the download and recovery of 'priority' keychain items.
 *     This is intended to be called soon after successfully joining into this clique.
 * @param error     An error parameter: filled in if the call times out, or recovery was unsuccessful
 * @return BOOL     Whether or not the wait was successful
 */
- (BOOL)waitForPriorityViewKeychainDataRecovery:(NSError**)error;

/* *
 * @abstract                        Evaluate an escrow record's TLK recoverability
 * @param record                    The escrow record to evaluate
 * @param error                     An error parameter: filled in if the call times out or verification fails in any way
 * @return NSArray<NSString*>*      An array of recoverable keychain views by this record
 */
- (NSArray<NSString*>* _Nullable)tlkRecoverabilityForEscrowRecord:(OTEscrowRecord*)record error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGONTRUST_H
#endif
