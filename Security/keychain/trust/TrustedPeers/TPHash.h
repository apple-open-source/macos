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

NS_ASSUME_NONNULL_BEGIN

/*!
 A hash digest algorithm
 */
typedef NS_ENUM(NSInteger, TPHashAlgo) {
    kTPHashAlgoUnknown = -1,
    kTPHashAlgoSHA224 = 0,
    kTPHashAlgoSHA256,
    kTPHashAlgoSHA384,
    kTPHashAlgoSHA512,
};

/*!
 A hash prefixed with the name of the digest algorithm, e.g.
 "SHA256:xxxx" where the 'x' are 8-bit bytes.
 */


@interface TPHashBuilder : NSObject

+ (TPHashAlgo)algoOfHash:(NSString *)hash;

- (instancetype)initWithAlgo:(TPHashAlgo)algo;
- (void)resetWithAlgo:(TPHashAlgo)algo;
- (void)updateWithData:(NSData *)data;
- (void)updateWithBytes:(const void *)data len:(size_t)len;
- (NSString *)finalHash;

+ (NSString *)hashWithAlgo:(TPHashAlgo)algo ofData:(NSData *)data;
+ (NSString *)hashWithAlgo:(TPHashAlgo)algo ofBytes:(const void *)data len:(size_t)len;

@end

NS_ASSUME_NONNULL_END
