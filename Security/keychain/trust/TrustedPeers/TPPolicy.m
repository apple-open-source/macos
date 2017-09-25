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

#import "TPPolicy.h"
#import "TPCategoryRule.h"


@interface TPPolicy ()

@property (nonatomic, strong) NSArray<TPCategoryRule*> *modelToCategory;
@property (nonatomic, strong) NSDictionary<NSString*,NSSet<NSString*>*> *categoriesByView;
@property (nonatomic, strong) NSDictionary<NSString*,NSSet<NSString*>*> *introducersByCategory;

@end


@implementation TPPolicy

+ (instancetype)policyWithModelToCategory:(NSArray<TPCategoryRule*> *)modelToCategory
                         categoriesByView:(NSDictionary<NSString*,NSSet<NSString*>*> *)categoriesByView
                    introducersByCategory:(NSDictionary<NSString*,NSSet<NSString*>*> *)introducersByCategory
{
    TPPolicy *policy = [[TPPolicy alloc] init];
    policy.modelToCategory = [modelToCategory copy];
    policy.categoriesByView = [categoriesByView copy];
    policy.introducersByCategory = [introducersByCategory copy];
    return policy;
}

- (nullable NSString *)categoryForModel:(NSString *)model
{
    for (TPCategoryRule *rule in self.modelToCategory) {
        if ([model hasPrefix:rule.prefix]) {
            return rule.category;
        }
    }
    return nil;
}

- (BOOL)trustedPeerInCategory:(NSString *)trustedCategory canIntroduceCategory:(NSString *)candidateCategory
{
    return [self.introducersByCategory[candidateCategory] containsObject:trustedCategory];
}

- (BOOL)peerInCategory:(NSString *)category canAccessView:(NSString *)view
{
    return [self.categoriesByView[view] containsObject:category];
}

@end
