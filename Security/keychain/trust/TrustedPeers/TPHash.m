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

#import "TPHash.h"

#import <CommonCrypto/CommonDigest.h>

@interface TPHashBuilder ()

@property (nonatomic, assign) TPHashAlgo algo;
@property (nonatomic, assign) CC_SHA256_CTX ctxSHA256; // used by SHA224 and SHA256
@property (nonatomic, assign) CC_SHA512_CTX ctxSHA512; // used by SHA384 and SHA512

@end

@implementation TPHashBuilder

+ (TPHashAlgo)algoOfHash:(NSString *)hash
{
    if ([hash hasPrefix:@"SHA224:"]) {
        return kTPHashAlgoSHA224;
    }
    if ([hash hasPrefix:@"SHA256:"]) {
        return kTPHashAlgoSHA256;
    }
    if ([hash hasPrefix:@"SHA384:"]) {
        return kTPHashAlgoSHA384;
    }
    if ([hash hasPrefix:@"SHA512:"]) {
        return kTPHashAlgoSHA512;
    }
    return kTPHashAlgoUnknown;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _algo = kTPHashAlgoUnknown;
    }
    return self;
}

- (instancetype)initWithAlgo:(TPHashAlgo)algo
{
    self = [self init];
    [self resetWithAlgo:algo];
    return self;
}

- (void)resetWithAlgo:(TPHashAlgo)algo
{
    _algo = algo;
    switch (algo) {
        case kTPHashAlgoSHA224:
            CC_SHA224_Init(&_ctxSHA256);
            break;
        case kTPHashAlgoSHA256:
            CC_SHA256_Init(&_ctxSHA256);
            break;
        case kTPHashAlgoSHA384:
            CC_SHA384_Init(&_ctxSHA512);
            break;
        case kTPHashAlgoSHA512:
            CC_SHA512_Init(&_ctxSHA512);
            break;
        default:
            [self throwInvalidAlgo];
    }
}

- (void)updateWithData:(NSData *)data
{
    [self updateWithBytes:data.bytes len:data.length];
}

- (void)updateWithBytes:(const void *)data len:(size_t)len
{
    switch (self.algo) {
        case kTPHashAlgoSHA224:
            CC_SHA224_Update(&_ctxSHA256, data, (CC_LONG)len);
            break;
        case kTPHashAlgoSHA256:
            CC_SHA256_Update(&_ctxSHA256, data, (CC_LONG)len);
            break;
        case kTPHashAlgoSHA384:
            CC_SHA384_Update(&_ctxSHA512, data, (CC_LONG)len);
            break;
        case kTPHashAlgoSHA512:
            CC_SHA512_Update(&_ctxSHA512, data, (CC_LONG)len);
            break;
        default:
            [self throwInvalidAlgo];
    }
}

- (NSString *)finalHash
{
    NSMutableData* data = [NSMutableData alloc];
    NSString* name = nil;
    switch (self.algo) {
        case kTPHashAlgoSHA224:
            data = [data initWithLength:224/8];
            CC_SHA224_Final(data.mutableBytes, &_ctxSHA256);
            name = @"SHA224";
            break;
        case kTPHashAlgoSHA256:
            data = [data initWithLength:256/8];
            CC_SHA256_Final(data.mutableBytes, &_ctxSHA256);
            name = @"SHA256";
            break;
        case kTPHashAlgoSHA384:
            data = [data initWithLength:384/8];
            CC_SHA384_Final(data.mutableBytes, &_ctxSHA512);
            name = @"SHA384";
            break;
        case kTPHashAlgoSHA512:
            data = [data initWithLength:512/8];
            CC_SHA512_Final(data.mutableBytes, &_ctxSHA512);
            name = @"SHA512";
            break;
        default:
            [self throwInvalidAlgo];
    }
    NSString* hash = [NSString stringWithFormat:@"%@:%@",
                      name, [data base64EncodedStringWithOptions:0]];
    
    // _ctxSHA* was "emptied" by the call to CC_SHA*_Final,
    // so require the client to call resetWithAlgo: before reuse.
    self.algo = kTPHashAlgoUnknown;
    
    return hash;
}

- (void)throwInvalidAlgo
{
    NSException* ex = [NSException exceptionWithName:@"InvalidTPHashAlgo"
                                              reason:@"Invalid TPHash algorithm"
                                            userInfo:nil];
    @throw ex;
}

+ (NSString *)hashWithAlgo:(TPHashAlgo)algo ofData:(NSData *)data
{
    return [TPHashBuilder hashWithAlgo:algo ofBytes:data.bytes len:data.length];
}

+ (NSString *)hashWithAlgo:(TPHashAlgo)algo ofBytes:(const void *)data len:(size_t)len
{
    TPHashBuilder *builder = [[TPHashBuilder alloc] initWithAlgo:algo];
    [builder updateWithBytes:data len:len];
    return [builder finalHash];
}

@end
