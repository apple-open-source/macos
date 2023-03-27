/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

@class OTAccountSettings;
@class OTOperationDependencies;
@class CuttlefishXPCWrapper;
@class TPSpecificUser;

NS_ASSUME_NONNULL_BEGIN

@protocol OTAccountSettingsContainer
- (void)setAccountSettings:(OTAccountSettings*_Nullable)accountSettings;
@end

@interface OTStashAccountSettingsOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                     accountSettings:(id<OTAccountSettingsContainer>)accountSettings
                         accountWide:(bool)accountWide
                          forceFetch:(bool)forceFetch;

+ (void)performWithAccountWide:(bool)accountWide
                    forceFetch:(bool)forceFetch
          cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
                 activeAccount:(TPSpecificUser* _Nullable)activeAccount
                 containerName:(NSString*)containerName
                     contextID:(NSString*)contextID
                         reply:(void (^)(OTAccountSettings* _Nullable settings, NSError* _Nullable error))reply;

@end

NS_ASSUME_NONNULL_END

#endif
