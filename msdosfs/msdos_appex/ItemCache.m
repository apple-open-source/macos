/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

#import "FATManager.h"
#import "FATVolume.h"
#import "ItemCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface ItemCache()

@property NSMutableDictionary *itemsHash;
@property FATVolume *volume;

@end


@implementation ItemCache

-(instancetype _Nullable)initWithVolume:(FATVolume *)volume
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _itemsHash = [[NSMutableDictionary alloc]init];

    if (!_itemsHash) {
        os_log_error(OS_LOG_DEFAULT, "%s: Failed to initialize queue / hash", __func__);
        return nil;
    }

    _volume = volume;

    return self;
}

- (void)insertItem:(FATItem *)item
      replyHandler:(void (^)(FATItem * _Nullable cachedItem,
                             NSError * _Nullable error))reply
{
    __block FATItem *existingItem = nil;
    __block FATItem *itemToReturn = nil;
    __block uint64_t volumeOffset = 0;
    __block NSError *error = nil;
    NSString *key = nil;

    /* Make sure the item is not deleted */
    if (item.isDeleted) {
        os_log_fault(OS_LOG_DEFAULT, "%s: Item is deleted", __func__);
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    volumeOffset = [item.entryData calcFirstEntryOffsetInVolume:item.volume.systemInfo];

    key = [[NSString alloc] initWithFormat:@"%llu", volumeOffset];

    @synchronized (_itemsHash) {
        /* Make sure the item is not in the cache. If it is, return it */
        existingItem = [_itemsHash objectForKey:key];
        if (existingItem != nil) {
            if (existingItem.isDeleted) {
                /* Need to update the item */
                [_itemsHash setObject:item forKey:key];
                itemToReturn = item;
            } else {
                itemToReturn = existingItem;
                os_log_debug(OS_LOG_DEFAULT, "%s: Item already cached", __func__);
            }
        } else {
            /* Item is not in the cache. Insert it. */
            [_itemsHash setObject:item forKey:key];
        }
    };

    return reply(itemToReturn ? itemToReturn : item, error);
}

-(void)removeItem:(FATItem*)item
{
    __block uint64_t volumeOffset = [item.entryData calcFirstEntryOffsetInVolume:item.volume.systemInfo];
    __block FATItem *cachedItem = nil;

    NSString *key = [[NSString alloc] initWithFormat:@"%llu", volumeOffset];

    @synchronized (_itemsHash) {
        cachedItem = [_itemsHash objectForKey:key];
        if (cachedItem == nil) {
            /* TODO: Do we want to fault here? */
            os_log_fault(OS_LOG_DEFAULT, "%s: item for key %@ not found", __func__, key);
        } else {
            [_itemsHash removeObjectForKey:key];
        }
    };
}

@end

NS_ASSUME_NONNULL_END
