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

#import "TPCategoryRule.h"


@interface TPCategoryRule ()

@property (nonatomic, copy) NSString *prefix;
@property (nonatomic, copy) NSString *category;

@end


@implementation TPCategoryRule

+ (instancetype)ruleWithPrefix:(NSString *)prefix category:(NSString *)category
{
    TPCategoryRule *rule = [[TPCategoryRule alloc] init];
    rule.prefix = prefix;
    rule.category = category;
    return rule;
}

- (BOOL)isEqualToCategoryRule:(TPCategoryRule *)other
{
    if (other == self) {
        return YES;
    }
    return [self.prefix isEqualToString:other.prefix]
        && [self.category isEqualToString:other.category];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPCategoryRule class]]) {
        return NO;
    }
    return [self isEqualToCategoryRule:object];
}

- (NSUInteger)hash
{
    return [self.prefix hash] ^ [self.category hash];
}

@end
