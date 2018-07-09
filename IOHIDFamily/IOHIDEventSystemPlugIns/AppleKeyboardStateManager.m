//
//  AppleKeyboardStateManager.m
//  AppleKeyboardFilter
//
//  Created by Daniel Kim on 3/22/18.
//  Copyright © 2018 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AppleKeyboardStateManager.h"

@interface AppleKeyboardStateManager()
@property (nonatomic) NSMutableSet <NSNumber *> *capsLockStateTable;
@end

@implementation AppleKeyboardStateManager

+ (instancetype)sharedManager {
    static AppleKeyboardStateManager *sharedManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedManager = [[self alloc] init];
    });
    return sharedManager;
}

-(instancetype)init
{
    self = [super init];
    
    if (!self) {
        return self;
    }
    
    _capsLockStateTable = [NSMutableSet new];
    
    return self;
}

- (BOOL)isCapsLockEnabled:(NSNumber *)locationID
{
    BOOL result = NO;
    
    if (!locationID) {
        return result;
    }
    
    @synchronized (self) {
        result = [_capsLockStateTable containsObject:locationID];
    }
    
    return result;
}

- (void)setCapsLockEnabled:(BOOL)enable
                locationID:(NSNumber *)locationID
{
    if (!locationID) {
        return;
    }
    
    @synchronized (self) {
        if (enable) {
            [_capsLockStateTable addObject:locationID];
        } else {
            [_capsLockStateTable removeObject:locationID];
        }
    }
}

@end
