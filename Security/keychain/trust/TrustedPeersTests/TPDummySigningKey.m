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

#import "TPDummySigningKey.h"

@interface TPDummySigningKey ()
@property (nonatomic, strong) NSData *publicKey;
@end


@implementation TPDummySigningKey

- (instancetype)initWithPublicKeyData:(NSData *)publicKey
{
    self = [super init];
    if (self) {
        _publicKey = publicKey;
        _privateKeyIsAvailable = YES;
    }
    return self;
}

- (NSData *)signatureForData:(NSData *)data withError:(NSError **)error
{
    if (self.privateKeyIsAvailable) {
        return [self signatureForData:data];
    } else {
        if (error) {
            *error = [NSError errorWithDomain:@"TPDummySigningKey" code:1 userInfo:nil];
        }
        return nil;
    }
}

- (NSData *)signatureForData:(NSData *)data
{
    // A really dumb hash that is just good enough for unit tests.
    NSUInteger hash = [self.publicKey hash] ^ [data hash];
    return [NSData dataWithBytes:&hash length:sizeof(hash)];
}

- (BOOL)checkSignature:(NSData *)sig matchesData:(NSData *)data
{
    return [sig isEqualToData:[self signatureForData:data]];
}

@end


@implementation TPDummySigningKeyFactory

- (id <TPSigningKey>)keyWithPublicKeyData:(NSData *)publicKey
{
    if (0 == publicKey.length) {
        return nil;
    }
    return [[TPDummySigningKey alloc] initWithPublicKeyData:publicKey];
}

+ (instancetype) dummySigningKeyFactory
{
    return [[TPDummySigningKeyFactory alloc] init];
}

@end
