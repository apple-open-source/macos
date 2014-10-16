//
//  NSArray+map.m
//  Security
//
//  Created by J Osborne on 3/8/13.
//  Copyright (c) 2013 Apple Inc. All Rights Reserved.
//

#import "NSArray+map.h"

@implementation NSArray (map)

-(NSArray*)mapWithBlock:(mapBlock)block
{
    NSMutableArray *results = [[NSMutableArray alloc] initWithCapacity:self.count];
    
    for (id obj in self) {
        [results addObject:block(obj)];
    }
    
    return [results copy];
}

@end
