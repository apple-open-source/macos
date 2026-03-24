/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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


#import "NSError+UsefulConstructors.h"

@implementation NSError (UsefulConstructors)

+ (instancetype)errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code description:(NSString*)description {
    return [NSError errorWithDomain:domain code:code description:description underlying:nil];
}

+ (instancetype)errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code description:(NSString*)description underlying:(NSError*)underlying {
    // Obj-C throws a fit if there's nulls in dictionaries, so we can't use a dictionary literal here.
    // Use the null-assignment semantics of NSMutableDictionary to make a dictionary either with either, both, or neither key.
    NSMutableDictionary* mut = [[NSMutableDictionary alloc] init];
    mut[NSLocalizedDescriptionKey] = description;
    mut[NSUnderlyingErrorKey] = underlying;

    return [NSError errorWithDomain:domain code:code userInfo:mut];
}

- (NSString*)formatAsNestedJSON {
    // Handle nil domain safely
    NSString *domain = self.domain ?: @"(null)";
    NSString *currentTuple = [NSString stringWithFormat:@"(%@,%ld)", domain, (long)self.code];
    NSMutableArray<NSString *> *childrenJSON = [NSMutableArray array];

    // Safely access userInfo (could be nil)
    NSDictionary *userInfo = self.userInfo;
    if (userInfo) {
        // Process single underlying error
        id underlyingErrorObj = userInfo[NSUnderlyingErrorKey];
        if (underlyingErrorObj && [underlyingErrorObj isKindOfClass:[NSError class]]) {
            NSError *underlyingError = (NSError *)underlyingErrorObj;
            NSString *childJSON = [underlyingError formatAsNestedJSON];
            if (childJSON) {
                [childrenJSON addObject:childJSON];
            }
        }

        // Process multiple underlying errors
        id underlyingMultipleErrorsObj = userInfo[NSMultipleUnderlyingErrorsKey];
        if (underlyingMultipleErrorsObj && [underlyingMultipleErrorsObj isKindOfClass:[NSArray class]]) {
            NSArray *multipleErrorsArray = (NSArray *)underlyingMultipleErrorsObj;
            for (id errorObj in multipleErrorsArray) {
                if (errorObj && [errorObj isKindOfClass:[NSError class]]) {
                    NSError *underlyingMultipleError = (NSError *)errorObj;
                    NSString *childJSON = [underlyingMultipleError formatAsNestedJSON];
                    if (childJSON) {
                        [childrenJSON addObject:childJSON];
                    }
                }
            }
        }
    }

    if (childrenJSON.count == 0) {
        return [NSString stringWithFormat:@"{%@:{}}", currentTuple];
    } else if (childrenJSON.count == 1) {
        return [NSString stringWithFormat:@"{%@:%@}", currentTuple, childrenJSON[0]];
    } else {
        NSString *childrenString = [NSString stringWithFormat:@"[%@]", [childrenJSON componentsJoinedByString:@","]];
        return [NSString stringWithFormat:@"{%@:%@}", currentTuple, childrenString];
    }
}

@end
