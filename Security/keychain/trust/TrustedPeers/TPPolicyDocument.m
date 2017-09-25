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

#import "TPPolicyDocument.h"
#import "TPPolicy.h"
#import "TPUtils.h"
#import "TPCategoryRule.h"

static const NSString *kPolicyVersion = @"policyVersion";
static const NSString *kModelToCategory = @"modelToCategory";
static const NSString *kCategoriesByView = @"categoriesByView";
static const NSString *kIntroducersByCategory = @"introducersByCategory";
static const NSString *kRedactions = @"redactions";
static const NSString *kPrefix = @"prefix";
static const NSString *kCategory = @"category";

@interface TPPolicyDocument ()

@property (nonatomic, assign) TPCounter policyVersion;
@property (nonatomic, strong) NSString *policyHash;
@property (nonatomic, strong) NSData *pList;

@property (nonatomic, strong) NSArray<TPCategoryRule*> *modelToCategory;
@property (nonatomic, strong) NSDictionary<NSString*,NSSet<NSString*>*> *categoriesByView;
@property (nonatomic, strong) NSDictionary<NSString*,NSSet<NSString*>*> *introducersByCategory;
@property (nonatomic, strong) NSDictionary<NSString*,NSData*> *redactions;

@end


@implementation TPPolicyDocument

+ (nullable NSArray<TPCategoryRule*> *)modelToCategoryFromObj:(id)obj
{
    if (![obj isKindOfClass:[NSArray class]]) {
        return nil;
    }
    NSArray *arr = obj;
    NSMutableArray<TPCategoryRule*> *rules = [[NSMutableArray alloc] initWithCapacity:arr.count];
    for (id item in arr) {
        TPCategoryRule *rule = [self categoryRuleFromObj:item];
        if (nil == rule) {
            return nil;
        }
        [rules addObject:rule];
    }
    return rules;
}

+ (nullable TPCategoryRule *)categoryRuleFromObj:(id)obj
{
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSDictionary *dict = obj;
    if (![dict[kPrefix] isKindOfClass:[NSString class]]) {
        return nil;
    }
    if (![dict[kCategory] isKindOfClass:[NSString class]]) {
        return nil;
    }
    return [TPCategoryRule ruleWithPrefix:dict[kPrefix] category:dict[kCategory]];
}

// Used for parsing categoriesByView and introducersByCategory
// which both have the same structure.
+ (nullable NSDictionary<NSString*,NSSet<NSString*>*> *)dictionaryOfSetsFromObj:(id)obj
{
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSDictionary *dict = obj;
    NSMutableDictionary<NSString*,NSSet<NSString*>*> *result = [NSMutableDictionary dictionary];
    for (id key in dict) {
        if (![key isKindOfClass:[NSString class]]) {
            return nil;
        }
        id value = dict[key];
        if (![value isKindOfClass:[NSArray class]]) {
            return nil;
        }
        NSArray *arr = value;
        for (id item in arr) {
            if (![item isKindOfClass:[NSString class]]) {
                return nil;
            }
        }
        result[key] = [NSSet setWithArray:arr];
    }
    return result;
}

+ (nullable NSDictionary<NSString*,NSData*> *)redactionsFromObj:(id)obj
{
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSDictionary *dict = obj;
    for (id key in dict) {
        if (![key isKindOfClass:[NSString class]]) {
            return nil;
        }
        id value = dict[key];
        if (![value isKindOfClass:[NSData class]]) {
            return nil;
        }
    }
    return dict;
}

+ (nullable instancetype)policyDocWithHash:(NSString *)policyHash
                                     pList:(NSData *)pList
{
    TPHashAlgo algo = [TPHashBuilder algoOfHash:policyHash];
    NSString *hash = [TPHashBuilder hashWithAlgo:algo ofData:pList];
    if (![policyHash isEqualToString:hash]) {
        return nil;
    }
    TPPolicyDocument *doc = [[TPPolicyDocument alloc] init];
    doc.policyHash = hash;
    doc.pList = pList;
    
    id obj = [NSPropertyListSerialization propertyListWithData:pList
                                                        options:NSPropertyListImmutable
                                                         format:nil
                                                          error:NULL];
    if (![obj isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    NSDictionary *dict = obj;
    
    if (![dict[kPolicyVersion] isKindOfClass:[NSNumber class]]) {
        return nil;
    }
    doc.policyVersion = [dict[kPolicyVersion] unsignedLongLongValue];

    doc.modelToCategory = [self modelToCategoryFromObj:dict[kModelToCategory]];
    if (nil == doc.modelToCategory) {
        return nil;
    }
    doc.categoriesByView = [self dictionaryOfSetsFromObj:dict[kCategoriesByView]];
    if (nil == doc.categoriesByView) {
        return nil;
    }
    doc.introducersByCategory = [self dictionaryOfSetsFromObj:dict[kIntroducersByCategory]];
    if (nil == doc.introducersByCategory) {
        return nil;
    }
    doc.redactions = [self redactionsFromObj:dict[kRedactions]];
    if (nil == doc.redactions) {
        return nil;
    }
    return doc;
}

+ (instancetype)policyDocWithVersion:(TPCounter)policyVersion
                     modelToCategory:(NSArray<NSDictionary*> *)modelToCategory
                    categoriesByView:(NSDictionary<NSString*,NSArray<NSString*>*> *)categoriesByView
               introducersByCategory:(NSDictionary<NSString*,NSArray<NSString*>*> *)introducersByCategory
                          redactions:(NSDictionary<NSString*,NSData*> *)redactions
                            hashAlgo:(TPHashAlgo)hashAlgo
{
    TPPolicyDocument *doc = [[TPPolicyDocument alloc] init];
    
    doc.policyVersion = policyVersion;
    
    doc.modelToCategory = [TPPolicyDocument modelToCategoryFromObj:modelToCategory];
    NSAssert(doc.modelToCategory, @"malformed modelToCategory");
    
    doc.categoriesByView = [TPPolicyDocument dictionaryOfSetsFromObj:categoriesByView];
    NSAssert(doc.categoriesByView, @"malformed categoriesByView");
    
    doc.introducersByCategory = [TPPolicyDocument dictionaryOfSetsFromObj:introducersByCategory];
    NSAssert(doc.introducersByCategory, @"malformed introducersByCategory");
    
    doc.redactions = [redactions copy];
    
    NSDictionary *dict = @{
                           kPolicyVersion: @(policyVersion),
                           kModelToCategory: modelToCategory,
                           kCategoriesByView: categoriesByView,
                           kIntroducersByCategory: introducersByCategory,
                           kRedactions: redactions
                           };
    doc.pList = [TPUtils serializedPListWithDictionary:dict];
    doc.policyHash = [TPHashBuilder hashWithAlgo:hashAlgo ofData:doc.pList];
    
    return doc;
}

+ (nullable NSData *)redactionWithEncrypter:(id<TPEncrypter>)encrypter
                            modelToCategory:(nullable NSArray<NSDictionary*> *)modelToCategory
                           categoriesByView:(nullable NSDictionary<NSString*,NSArray<NSString*>*> *)categoriesByView
                      introducersByCategory:(nullable NSDictionary<NSString*,NSArray<NSString*>*> *)introducersByCategory
                                      error:(NSError **)error
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (nil != modelToCategory) {
        dict[kModelToCategory] = modelToCategory;
    }
    if (nil != categoriesByView) {
        dict[kCategoriesByView] = categoriesByView;
    }
    if (nil != introducersByCategory) {
        dict[kIntroducersByCategory] = introducersByCategory;
    }
    NSData *plist = [TPUtils serializedPListWithDictionary:dict];
    return [encrypter encryptData:plist error:error];
}

- (id<TPPolicy>)policyWithSecrets:(NSDictionary<NSString*,NSData*> *)secrets
                        decrypter:(id<TPDecrypter>)decrypter
                            error:(NSError **)error
{
    NSArray<TPCategoryRule*> *modelToCategory = self.modelToCategory;
    NSMutableDictionary<NSString*,NSSet<NSString*>*> *categoriesByView
        = [NSMutableDictionary dictionaryWithDictionary:self.categoriesByView];
    NSMutableDictionary<NSString*,NSSet<NSString*>*> *introducersByCategory
        = [NSMutableDictionary dictionaryWithDictionary:self.introducersByCategory];
    
    // We are going to prepend extra items to modelToCategory.
    // To make the resulting array order deterministic we sort secrets by name first.
    NSArray<NSString*> *names = [secrets.allKeys sortedArrayUsingSelector:@selector(compare:)];
    for (NSString *name in names) {
        NSData *key = secrets[name];
        NSData *ciphertext = self.redactions[name];
        if (nil == ciphertext) {
            // This is normal. A new version might have no need to redact
            // info that was revealed by keys for a previous version.
            continue;
        }
        NSData *plist = [decrypter decryptData:ciphertext withKey:key error:error];
        if (nil == plist) {
            return nil;
        }
        id obj = [NSPropertyListSerialization propertyListWithData:plist
                                                           options:NSPropertyListImmutable
                                                            format:nil
                                                             error:NULL];
        if (![obj isKindOfClass:[NSDictionary class]]) {
            return nil;
        }
        NSDictionary *dict = obj;
        
        NSArray<TPCategoryRule*> *extraModelToCategory;
        extraModelToCategory = [TPPolicyDocument modelToCategoryFromObj:dict[kModelToCategory]];
        if (nil != extraModelToCategory) {
            // Extra rules are prepended to the list so that they are considered first.
            modelToCategory = [extraModelToCategory arrayByAddingObjectsFromArray:modelToCategory];
        }

        NSDictionary<NSString*,NSSet<NSString*>*> *extraCategoriesByView;
        extraCategoriesByView = [TPPolicyDocument dictionaryOfSetsFromObj:dict[kCategoriesByView]];
        if (nil != extraCategoriesByView) {
            [self mergeExtras:extraCategoriesByView intoDictionary:categoriesByView];
        }
        
        NSDictionary<NSString*,NSSet<NSString*>*> *extraIntroducersByCategory;
        extraIntroducersByCategory = [TPPolicyDocument dictionaryOfSetsFromObj:dict[kIntroducersByCategory]];
        if (nil != extraIntroducersByCategory) {
            [self mergeExtras:extraIntroducersByCategory intoDictionary:introducersByCategory];
        }
    }
    
    return [TPPolicy policyWithModelToCategory:modelToCategory
                              categoriesByView:categoriesByView
                         introducersByCategory:introducersByCategory];
}

- (void)mergeExtras:(NSDictionary<NSString*,NSSet<NSString*>*> *)extras
     intoDictionary:(NSMutableDictionary<NSString*,NSSet<NSString*>*> *)target
{
    for (NSString *name in extras) {
        NSSet<NSString*>* extraSet = extras[name];
        if (target[name] == nil) {
            target[name] = extraSet;
        } else {
            target[name] = [target[name] setByAddingObjectsFromSet:extraSet];
        }
    }
}

- (BOOL)isEqualToPolicyDocument:(TPPolicyDocument *)other
{
    if (other == self) {
        return YES;
    }
    return self.policyVersion == other.policyVersion
        && [self.policyHash isEqualToString:other.policyHash]
        && [self.pList isEqualToData:other.pList]
        && [self.modelToCategory isEqualToArray:other.modelToCategory]
        && [self.categoriesByView isEqualToDictionary:other.categoriesByView]
        && [self.introducersByCategory isEqualToDictionary:other.introducersByCategory]
        && [self.redactions isEqualToDictionary:other.redactions];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPPolicyDocument class]]) {
        return NO;
    }
    return [self isEqualToPolicyDocument:object];
}

- (NSUInteger)hash
{
    return [self.policyHash hash];
}

@end
