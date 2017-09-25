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
 A protocol for signing blobs and checking signatures.
 */
@protocol TPSigningKey <NSObject>
- (NSData *)publicKey;
- (BOOL)checkSignature:(NSData *)sig matchesData:(NSData *)data;

/*!
 This method uses the private key to create a signature.
 It will return nil with an error if the private key is not available,
 e.g. due to the device being locked.
 */
- (nullable NSData *)signatureForData:(NSData *)data withError:(NSError **)error;
@end


/*!
 A protocol for factories that construct TPSigningKey objects.
 */
@protocol TPSigningKeyFactory <NSObject>
// Return nil if data is malformed
- (nullable id <TPSigningKey>)keyWithPublicKeyData:(NSData *)publicKey;
@end

NS_ASSUME_NONNULL_END
