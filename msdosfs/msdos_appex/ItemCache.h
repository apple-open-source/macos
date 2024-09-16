/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef FileIdentityCache_h
#define FileIdentityCache_h

#import "FATItem.h"

NS_ASSUME_NONNULL_BEGIN

@interface ItemCache : NSObject

-(instancetype _Nullable)initWithVolume:(FATVolume *)volume;

- (void)insertItem:(FATItem *)item
      replyHandler:(void (^)(FATItem * _Nullable cachedItem,
                             NSError * _Nullable error))reply;

-(void)removeItem:(FATItem*)item;

@end

NS_ASSUME_NONNULL_END

#endif /* FileIdentityCache_h */
