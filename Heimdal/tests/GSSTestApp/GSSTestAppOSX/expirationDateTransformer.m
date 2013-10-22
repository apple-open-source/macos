//
//  expirationDateTransformer.m
//  GSSTestApp
//
//  Created by Love Hörnquist Åstrand on 2013-07-03.
//  Copyright (c) 2013 Apple, Inc. All rights reserved.
//

#import <AppKit/AppKit.h>
#import "expirationDateTransformer.h"

@implementation expirationDateTransformer

+ (Class)transformedValueClass
{
    return [NSString class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (id)transformedValue:(NSDate *)value
{
    
    if (value == nil) return @"expired";
    
    if ([value compare:[NSDate date]] != NSOrderedDescending)
        return @"expired";

    return [NSDateFormatter dateFormatFromTemplate:@"yyyyMMDD HH:MM" options:0 locale:[NSLocale currentLocale]];
}


@end
