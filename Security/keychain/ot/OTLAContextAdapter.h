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
#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <AppleKeyStore/AppleKeyStoreDefs.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTLAContextAdapter
- (BOOL)setCredential:(NSData *)credential type:(LACredentialType)type laContext:(LAContext*_Nonnull*_Nonnull)laContext error:(NSError **)error NS_SWIFT_NOTHROW;
- (void)discardPasscodeStashSecret:(cache_flow_enabled_context_t)contextType;
@end

@interface OTLAContextActualAdapter : NSObject <OTLAContextAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
