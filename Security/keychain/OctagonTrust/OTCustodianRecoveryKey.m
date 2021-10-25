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

#if __OBJC2__

#import "keychain/OctagonTrust/OTCustodianRecoveryKey.h"

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFEncryptionOperation.h>

@implementation OTCustodianRecoveryKey

- (BOOL)generateWrappingWithError:(NSError**)error {
    SFAESKeySpecifier* specifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAESKey* aesKey = [[SFAESKey alloc] initRandomKeyWithSpecifier:specifier error:error];
    if (aesKey == nil) {
        return NO;
    }

    _wrappingKey = aesKey.keyData;

    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:specifier];
    SFAuthenticatedCiphertext* cipher = [op encrypt:[_recoveryString dataUsingEncoding:NSUTF8StringEncoding] withKey:aesKey additionalAuthenticatedData:[NSData data] error:error];
    if (cipher == nil) {
        return NO;
    }

    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:cipher requiringSecureCoding:YES error:error];
    if (data == nil) {
        return NO;
    }

    _wrappedKey = data;
    return YES;
}

- (nullable instancetype)initWithUUID:(NSUUID*)uuid recoveryString:(NSString*)recoveryString error:(NSError**)error {
    if ((self = [super init]) ) {
        NSError *localError = nil;

        _uuid = uuid;
        _recoveryString = recoveryString;
        if (![self generateWrappingWithError:&localError]) {
            if (error) {
                *error = localError;
            }
            return nil;
        }
    }
    return self;
}

- (BOOL)unwrapWithError:(NSError**)error {
    SFAESKeySpecifier* specifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAESKey* aesKey = [[SFAESKey alloc] initWithData:_wrappingKey specifier:specifier error:error];
    if (aesKey == nil) {
        return NO;
    }

    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:specifier];
    SFAuthenticatedCiphertext* cipher = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_wrappedKey error:error];
    if (cipher == nil) {
        return NO;
    }

    NSData* data = [op decrypt:cipher withKey:aesKey error:error];
    if (data == nil) {
        return NO;
    }
    _recoveryString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return YES;
}

- (nullable instancetype)initWithWrappedKey:(NSData*)wrappedKey wrappingKey:(NSData*)wrappingKey uuid:(NSUUID*)uuid error:(NSError**)error {
    if ((self = [super init])) {
        _uuid = uuid;
        _wrappedKey = wrappedKey;
        _wrappingKey = wrappingKey;

        if (![self unwrapWithError:error]) {
            return nil;
        }
    }
    return self;
}

- (BOOL)isEqualToCustodianRecoveryKey:(OTCustodianRecoveryKey *)other {
    if (self == other) {
        return YES;
    }
    return [self.uuid isEqual:other.uuid] &&
        [self.wrappingKey isEqualToData:other.wrappingKey] &&
        [self.wrappedKey isEqualToData:other.wrappedKey] &&
        [self.recoveryString isEqualToString:other.recoveryString];
}

- (BOOL)isEqual:(nullable id)object {
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[OTCustodianRecoveryKey class]]) {
        return NO;
    }
    return [self isEqualToCustodianRecoveryKey:object];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    if (self = [super init]) {
        _uuid = [coder decodeObjectOfClass:[NSUUID class] forKey:@"uuid"];
        _wrappingKey = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappingKey"];
        _wrappedKey = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappedKey"];
        _recoveryString = [coder decodeObjectOfClass:[NSString class] forKey:@"recoveryString"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
    [coder encodeObject:_uuid forKey:@"uuid"];
    [coder encodeObject:_wrappingKey forKey:@"wrappingKey"];
    [coder encodeObject:_wrappedKey forKey:@"wrappedKey"];
    [coder encodeObject:_recoveryString forKey:@"recoveryString"];
}

@end

#endif /* OBJC2 */
