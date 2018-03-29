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

#ifndef OTEscrow_h
#define OTEscrow_h
#if OCTAGON

#import <Foundation/Foundation.h>
#import <SecurityFoundation/SFKey.h>
NS_ASSUME_NONNULL_BEGIN

typedef enum {
    kOTEscrowKeySigning = 1,
    kOTEscrowKeyEncryption = 2,
    kOTEscrowKeySymmetric = 3,
} escrowKeyType;

@interface OTEscrowKeys : NSObject

@property (nonatomic, readonly) SFECKeyPair* encryptionKey;
@property (nonatomic, readonly) SFECKeyPair* signingKey;
@property (nonatomic, readonly) SFAESKey* symmetricKey;

@property (nonatomic, readonly) NSData* secret;
@property (nonatomic, readonly) NSString* dsid;

-(instancetype) init NS_UNAVAILABLE;

- (nullable instancetype) initWithSecret:(NSData*)secret
                                    dsid:(NSString*)dsid
                                   error:(NSError* __autoreleasing *)error;

+ (SecKeyRef) createSecKey:(NSData*)keyData;
+ (BOOL) setKeyMaterialInKeychain:(NSDictionary*)query error:(NSError* __autoreleasing *)error;

+ (NSData* _Nullable) generateEscrowKey:(escrowKeyType)keyType
                          masterSecret:(NSData*)masterSecret
                                  dsid:(NSString *)dsid
                                 error:(NSError**)error;

@end
NS_ASSUME_NONNULL_END
#endif
#endif /* OTEscrow_h */
