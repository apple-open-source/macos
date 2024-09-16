/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef DirNameCache_h
#define DirNameCache_h

#import "ExtensionCommon.h"

NS_ASSUME_NONNULL_BEGIN

typedef struct NameCacheEntry_s {
    uint32_t hash;
    uint32_t indexInDir;
} NameCacheEntry;

@interface NameCacheBucket : NSObject {
    @public NameCacheEntry * _Nullable elements;
}
@property uint32_t currCount;
@property uint32_t currSize;

@end


@interface DirNameCache : NSObject
@property NSMutableDictionary *nameCacheBuckets;
@property (atomic) bool isInUse;
@property (atomic) bool isIncomplete;
/* Save this property of the DirItem to convert a 64b offset to an index*/
@property uint32_t dirEntrySize;

/*
 * Initialize a DNC with the dir entry size of the dir, so we can convert between
 * an entry's offset (64b) and its index in the dir (32b).
 */
-(instancetype _Nullable)initWithDirEntrySize:(uint32_t)dirEntrySize;
/** Add an entry to the cache.
 @param chrString The entry's utf8 name
 @param stringLen The string's length
 @param offset The entry's offset in the directory
 @return Nil on success, the appropriate NSError in case an error happened.
 */
-(NSError * _Nullable)insertDirEntryNamed:(char *)chrString
                                 ofLength:(size_t)stringLen
                              offsetInDir:(uint64_t)offset;
/** Remove the entry from the cache.
 @param chrString The entry's utf8 name
 @param stringLen The string's length
 @param offset The entry's offset
 */
-(void)removeDirEntryNamed:(char *)chrString
                  ofLength:(size_t)stringLen
               offsetInDir:(uint64_t)offset;
/** Lookup an entry to get its offset in the directory.
 If there are several hashed values with the same hash(name) result, the first
 one will be returned.
 @param chrString The entry's utf8 name
 @param stringLen The string's length
 @param reply Reply block with the error (nil on success) and the entry offset.
 */
-(void)lookupDirEntryNamed:(char *)chrString
                  ofLength:(size_t)stringLen
              replyHandler:(void(^)(NSError *error,
                                    uint64_t entryOffsetInDir)) reply;

/** Add an entry to the cache.
 @param name The entry's utf16 name
 @param offset The entry's offset in the directory
 @return Nil on success, the appropriate NSError in case an error happened.
 */
-(NSError * _Nullable)insertDirEntryNamedUtf16:(struct unistr255 *)name
                                   offsetInDir:(uint64_t)offset;
/** Remove the entry from the cache.
 @param name The entry's utf16 name
 @param offset The entry's offset
 */
-(void)removeDirEntryNamedUtf16:(struct unistr255 *)name
                    offsetInDir:(uint64_t)offset;
/** Lookup an entry to get its offset in the directory.
 If there are several hashed values with the same hash(name) result, the first
 one will be returned.
 @param name The entry's utf16 name
 @param reply Reply block with the error (nil on success) and the entry offset.
 */
-(void)lookupDirEntryNamedUtf16:(struct unistr255 *)name
                   replyHandler:(void(^)(NSError *error,
                                         uint64_t entryOffsetInDir)) reply;


@end


@interface DNCPoolEntry : NSObject
@end

@class DirItem;

/**
 The DirNameCachePool serves as a LRU cache for directory name caches. It limits
 the number of DNCs we have at any given moment to keep our memory footprint at
 bay.
 When a new DNC is needed, the LRU entry which is not in use is evacuated and a
 new DNC is being allocated that slot.
 */
@interface DirNameCachePool : NSObject

/**
 Retrieves a DirNameCache from the main cache. The cache is limited in space,
 so it has up to <size> entries, <size> being defined in initWithSize.
 Once a DNC is retrieved, it will not be purged from the cache until
 doneWithNameCacheForDir will be called for it. The number of DirNameCache instances
 in the main cache is limited. getNameCacheForDir will wait for one of the instances to be available
 but after some time it will give up and will return nil DirNameCache with no error.

 @param dir The directory for which to retrieve a cache
 @param cachedOnly If true and no cache exists for that directory, don't create a new cache
 @param reply Reply block which returns an error (nil on success), the name
            cache (or nil) and whether or not this cache is new or was already
            cached.
 */
-(void)getNameCacheForDir:(DirItem*)dir
               cachedOnly:(bool)cachedOnly /* If true, don't create a new table if there isn't one already */
             replyHandler:(void(^)(NSError * _Nullable error,
                                   DirNameCache * _Nullable cache,
                                   bool isNew))reply;

/**
 Remove a DNC from the cache.
 The cache frees the LRU entries when needed, but this method allows users to
 actively reduce the cache's size.

 @param dir The directory for which to find and remove the name cache.
 */

/**
 Assumption: Called while holding the DNC
 Mark the DNC as not being used and freeing a cache slot to be used by other threads.
 This makes the DNC purgeable.

 @param dir The directory for which the DNC is no longer in use
 */
-(void)removeNameCacheForDir:(nonnull DirItem *)dir;

/**
 Assumption: Called while holding the DNC
 Mark the DNC as not being used and freeing a cache slot to be used by other threads.
 This makes the DNC purgeable.

 @param dir The directory for which to find and mark the cache as not in use.
 */
-(void)doneWithNameCacheForDir:(DirItem*)dir;
-(void)check;

@end

NS_ASSUME_NONNULL_END

#endif /* DirNameCache_h */
