/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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

/*!
    @header SecItemFetchOutOfBandPriv
    SecItemFetchOutOfBandPriv defines private Objective-C types and SPI functions for fetching PCS items, bypassing the state machine.
*/

#ifndef _SECURITY_SECITEMFETCHOUTOFBANDPRIV_H_
#define _SECURITY_SECITEMFETCHOUTOFBANDPRIV_H_

#ifdef __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCurrentItemQuery : NSObject <NSSecureCoding>
@property (nullable, strong) NSString* identifier;
@property (nullable, strong) NSString* accessGroup;
@property (nullable, strong) NSString* zoneID;

- (instancetype)initWithIdentifier:(NSString*)identifier accessGroup:(NSString*)accessGroup zoneID:(NSString*)zoneID;
@end

@interface CKKSCurrentItemQueryResult : NSObject <NSSecureCoding>
@property (nullable, strong) NSString* identifier;
@property (nullable, strong) NSString* accessGroup;
@property (nullable, strong) NSString* zoneID;
@property (nullable, strong) NSDictionary* decryptedRecord;

- (instancetype)initWithIdentifier:(NSString*)identifier accessGroup:(NSString*)accessGroup zoneID:(NSString*)zoneID decryptedRecord:(NSDictionary*)decryptedRecord;
@end

@interface CKKSPCSIdentityQuery : NSObject <NSSecureCoding>
@property (nullable, strong) NSNumber* serviceNumber;
@property (nullable, strong) NSString* accessGroup;
@property (nullable, strong) NSString* publicKey; // public key as a base-64 encoded string
@property (nullable, strong) NSString* zoneID;

- (instancetype)initWithServiceNumber:(NSNumber*)serviceNumber accessGroup:(NSString*)accessGroup publicKey:(NSString*)publicKey zoneID:(NSString*)zoneID;
@end

@interface CKKSPCSIdentityQueryResult : NSObject <NSSecureCoding>
@property (nullable, strong) NSNumber* serviceNumber;
@property (nullable, strong) NSString* publicKey; // public key as a base-64 encoded string
@property (nullable, strong) NSString* zoneID;
@property (nullable, strong) NSDictionary* decryptedRecord;

- (instancetype)initWithServiceNumber:(NSNumber*)serviceNumber publicKey:(NSString*)publicKey zoneID:(NSString*)zoneID decryptedRecord:(NSDictionary*)decryptedRecord;
@end

/*!
     @function secItemFetchCurrentItemOutOfBand
     @abstract Fetches, for the given array of CKKSCurrentItemQuery, the keychain items that are 'current' across this iCloud account from iCloud itself.
 @param currentItemQueries Array of CKKSCurrentItemQuery. Allows for querying multiple items at a time.
 @param forceFetch bool indicating whether results are force-fetched from CKDB
 @param complete Called to return values: Array of CKKSCurrentItemQueryResult which containes items decrypted and returned as a dictionary, accessed using the 'decryptedRecord' property if such items exist. Otherwise, error.
 */

void SecItemFetchCurrentItemOutOfBand(NSArray<CKKSCurrentItemQuery*>* currentItemQueries, bool forceFetch, void (^complete)(NSArray<CKKSCurrentItemQueryResult*>* currentItems, NSError* error));

/*!
     @function secItemFetchPCSIdentityOutOfBand
     @abstract Fetches, for the given array of CKKSPCSIdentityRequest, the item record referring to the PCS Identity in this iCloud account associated with the PCS service number and PCS public key from iCloud itself.
 @param pcsIdentityQueries Array of CKKSPCSIdentityRequest. Allows for querying multiple items at a time.
 @param forceFetch bool indicating whether results are force-fetched from CKDB
 @param complete Called to return values: Array of CKKSCurrentItemQueryResult which containes items decrypted and returned as a dictionary, accessed using the 'decryptedRecord' property if such items exist. Otherwise, error.
 */
void SecItemFetchPCSIdentityOutOfBand(NSArray<CKKSPCSIdentityQuery*>* pcsIdentityQueries, bool forceFetch, void (^complete)(NSArray<CKKSPCSIdentityQueryResult*>* pcsIdentities, NSError* error));

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* !_SECURITY_SECITEMFETCHOUTOFBANDPRIV_H_ */
