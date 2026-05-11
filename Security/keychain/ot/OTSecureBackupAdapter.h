/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

NS_ASSUME_NONNULL_BEGIN

// These properties already exist on `SecureBackup` objects, but including the CloudServices headers here causes Swift some heartburn.
// Make a protocol that SecureBackup can conform to, to allow access to these in tests.
@protocol OTSecureBackupEnablerProtocol
@property (nullable) NSData* passcodeStashSecret;
@property (nullable) NSData* entropy;
@property (nullable) NSString* bottleID;
@property (nullable) NSData* escrowSigningPublicKey;
@end


@protocol OTSecureBackupAdapter
- (BOOL)moveToFederationAllowed:(NSString*)federation altDSID:(NSString*)altDSID error:(NSError**)error NS_SWIFT_NOTHROW;
- (void)enableWithResults:(id<OTSecureBackupEnablerProtocol>)sb reply:(void (^)(NSDictionary* _Nullable, NSError* _Nullable))reply NS_SWIFT_NOTHROW;
- (BOOL)storeWithSecureBackup:(id<OTSecureBackupEnablerProtocol>)sb escrowRecord:(OTSerializedPlistEscrowRecord*)escrowRecord error:(NSError**)error NS_SWIFT_NOTHROW;
- (NSDictionary*)getAccountInfoWithSecureBackup:(id)sb error:(NSError**)error NS_SWIFT_NOTHROW;
@end

@interface OTSecureBackupActualAdapter : NSObject <OTSecureBackupAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
