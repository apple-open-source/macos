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
        os_log_error(fskit_std_log(), "%s: Failed to initialize queue / hash", __func__);
        return nil;
    }

    _volume = volume;

    return self;
}

- (void)calculateKeyForItem:(FATItem *)item
               replyHandler:(void (^)(uint64_t key,
                                      NSError * _Nullable error))reply
{
    __block uint64_t itemOffsetInDir = item.entryData.firstEntryOffsetInDir;
    uint32_t clusterSize = _volume.systemInfo.bytesPerCluster;
    __block uint64_t offsetInVolume = 0;
    __block uint64_t accOffset = 0;
    __block NSError *error = nil;

    /* Root dir is a special case, leave its key to be 0 */
    if (item.parentDir != nil) {
        /*
         * Calculate the key - the offset of the first direntry in the volume
         * divided by the size of direntry
         */
        [_volume.fatManager iterateClusterChainOfItem:item.parentDir
                                         replyHandler:^iterateClustersResult(NSError *innerError,
                                                                             uint32_t startCluster,
                                                                             uint32_t numOfContigClusters) {
            if (innerError) {
                error = innerError;
                return iterateClustersStop;
            } else {
                if (numOfContigClusters == 0) {
                    /* Item has no clusters, no key */
                    return iterateClustersStop;
                }
                if (itemOffsetInDir - accOffset + sizeof(struct dosdirentry) > numOfContigClusters * clusterSize) {
                    /* Our offset is not in this batch of contiguous clusters */
                    accOffset += numOfContigClusters * clusterSize;
                    return iterateClustersContinue;
                } else {
                    /* We are in the correct clusters range */
                    offsetInVolume = (itemOffsetInDir - accOffset) + startCluster * clusterSize;
                    return iterateClustersStop;
                }
            }
        }];
    }

    if (error) {
        /* We failed to find the volume index */
        return reply(0, error);
    }

    return reply(offsetInVolume, nil);
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
        os_log_fault(fskit_std_log(), "%s: Item is deleted", __func__);
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
                os_log_debug(fskit_std_log(), "%s: Item already cached", __func__);
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
            os_log_fault(fskit_std_log(), "%s: item for key %@ not found", __func__, key);
        } else {
            [_itemsHash removeObjectForKey:key];
        }
    };
}

@end

NS_ASSUME_NONNULL_END
