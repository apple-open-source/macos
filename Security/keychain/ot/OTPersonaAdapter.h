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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTPersonaAdapter
- (BOOL)currentThreadIsForPrimaryiCloudAccount;

// This should only be nil on platforms that do not support personas.
- (NSString* _Nullable)currentThreadPersonaUniqueString;

// If nil, this will revert to the default persona.
- (void)prepareThreadForKeychainAPIUseForPersonaIdentifier:(NSString* _Nullable)personaUniqueString;
- (void)performBlockWithPersonaIdentifier:(NSString* _Nullable)personaUniqueString
                                    block:(void (^) (void)) block;
@end

@interface OTPersonaActualAdapter : NSObject <OTPersonaAdapter>
- (instancetype)init;
@end

NS_ASSUME_NONNULL_END
