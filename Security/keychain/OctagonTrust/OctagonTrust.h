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

#import <Foundation/Foundation.h>
#import <Security/OTClique.h>
#import <OctagonTrust/OTEscrowRecord.h>
#import <OctagonTrust/OTEscrowTranslation.h>
#import <OctagonTrust/OTEscrowAuthenticationInformation.h>
#import <OctagonTrust/OTICDPRecordContext.h>
#import <OctagonTrust/OTICDPRecordSilentContext.h>
#import <OctagonTrust/OTEscrowRecordMetadata.h>
#import <OctagonTrust/OTEscrowRecordMetadataClientMetadata.h>


NS_ASSUME_NONNULL_BEGIN

//! Project version number for OctagonTrust.
FOUNDATION_EXPORT double OctagonTrustVersionNumber;

//! Project version string for OctagonTrust.
FOUNDATION_EXPORT const unsigned char OctagonTrustVersionString[];

extern NSString* OTCKContainerName;

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

@end

NS_ASSUME_NONNULL_END

#endif
