//
//  NSArray+map.h
//  Security
//
//  Created by J Osborne on 3/8/13.
//  Copyright (c) 2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>

typedef id (^mapBlock)(id obj);

@interface NSArray (map)
-(NSArray*)mapWithBlock:(mapBlock)block;
@end
