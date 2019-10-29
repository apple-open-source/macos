/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

// For now at least, we'll support backups only on iOS and macOS
#define SECDB_BACKUPS_ENABLED ((TARGET_OS_OSX || TARGET_OS_IOS || TARGET_OS_IOSMAC) && !TARGET_OS_SIMULATOR && !TARGET_DARWINOS)

#if __OBJC2__
#import <Foundation/Foundation.h>
#if !TARGET_OS_BRIDGE   // Specifically needed until rdar://problem/40583882 lands
#import <SecurityFoundation/SFKey.h>
#endif
#import "SecAKSObjCWrappers.h"
#import "CheckV12DevEnabled.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SecDbBackupRecoveryType) {
    SecDbBackupRecoveryTypeInvalid = -1,
    SecDbBackupRecoveryTypeAKS = 1,
    SecDbBackupRecoveryTypeCylon = 2,
    SecDbBackupRecoveryTypeRecoveryKey = 3,
};

extern NSString* const KeychainBackupsErrorDomain;

typedef NS_ENUM(NSInteger, SecDbBackupErrorCode) {
    SecDbBackupUnknownError = -1,
    SecDbBackupSuccess = 0,
    SecDbBackupAKSFailure,
    SecDbBackupCryptoFailure,
    SecDbBackupWriteFailure,
    SecDbBackupDeserializationFailure,
    SecDbBackupSetupFailure,
    SecDbBackupNoBackupBagFound,
    SecDbBackupNoKCSKFound,
    SecDbBackupDuplicateBagFound,
    SecDbBackupMultipleDefaultBagsFound,
    SecDbBackupMalformedBagDataOnDisk,
    SecDbBackupMalformedKCSKDataOnDisk,
    SecDbBackupMalformedUUIDDataOnDisk,
    SecDbBackupUUIDMismatch,
    SecDbBackupDataMismatch,
    SecDbBackupUnknownOption,
    SecDbBackupKeychainLocked,
    SecDbBackupInvalidArgument,
    SecDbBackupNotSupported,
    SecDbBackupInternalError,

    SecDbBackupTestCodeFailure = 255,     // support code for testing is falling over somehow
};

@interface SecDbBackupWrappedItemKey : NSObject <NSSecureCoding>
@property (nonatomic) NSData* wrappedKey;
@property (nonatomic) NSData* baguuid;
@end

@interface SecDbBackupManager : NSObject

+ (instancetype)manager;
- (instancetype)init NS_UNAVAILABLE;

#if !TARGET_OS_BRIDGE   // Specifically needed until rdar://problem/40583882 lands
- (SecDbBackupWrappedItemKey* _Nullable)wrapItemKey:(SFAESKey*)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error;
#else
- (SecDbBackupWrappedItemKey* _Nullable)wrapItemKey:(id)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error;
#endif

- (void)verifyBackupIntegrity:(bool)lightweight
                   completion:(void (^)(NSDictionary<NSString*, NSString*>* results, NSError* _Nullable error))completion;

@end

NS_ASSUME_NONNULL_END
#endif      // __OBJC2__

// Declare C functions here

bool SecDbBackupCreateOrLoadBackupInfrastructure(CFErrorRef _Nullable * _Nonnull error);
