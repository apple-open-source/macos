/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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


#ifndef OTInheritanceKeyTest_h
#define OTInheritanceKeyTest_h

#if __OBJC2__

#import <Foundation/Foundation.h>
#import "keychain/OctagonTrust/OTInheritanceKey.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTInheritanceKey (Test)
+ (NSString* _Nullable)base32:(const unsigned char*)d len:(size_t)inlen;
+ (NSData* _Nullable)unbase32:(const unsigned char*)s len:(size_t)inlen;
+ (NSString* _Nullable)printableWithData:(NSData*)data checksumSize:(size_t)checksumSize error:(NSError**)error;
+ (NSData* _Nullable)parseBase32:(NSString*)in checksumSize:(size_t)checksumSize error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif /* OBJC2 */

#endif /* OTInheritanceKeyTest_h */
