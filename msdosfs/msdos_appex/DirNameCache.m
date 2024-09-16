/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <dispatch/semaphore.h>
#import <zlib.h>

#import "DirNameCache.h"
#import "DirItem.h"

NS_ASSUME_NONNULL_BEGIN

#define MAX_NUM_BUCKETS (64)
#define MAX_HASH_VALUES (50000)
#define INCREMENTAL_ARRAY_LEN 100
/*
 * In order to reduce our memory footprint, limit how many DirNameCache objects
 * can exist at the same time.
 */
#define DIR_NAME_CACHES_LIMIT (5)


@implementation NameCacheBucket

-(instancetype _Nullable)init
{
    self = [super init];
    if (self) {
        self->elements = (NameCacheEntry*)calloc(INCREMENTAL_ARRAY_LEN, sizeof(NameCacheEntry));
        _currSize = INCREMENTAL_ARRAY_LEN;
        _currCount = 0;
    }

    return self;
}

-(void)dealloc
{
    if (self->elements != NULL) {
        free(self->elements);
        self->elements = NULL;
    }
}

-(int)addEntry:(NameCacheEntry)newEntry
{
    if (_currCount == _currSize) {
        /* Need to realloc */
        self->elements = realloc(self->elements, (self.currSize + INCREMENTAL_ARRAY_LEN) * sizeof(NameCacheEntry));
        if (!self->elements) {
            return ENOMEM;
        }
        _currSize += INCREMENTAL_ARRAY_LEN;
    }
    self->elements[_currCount++] = newEntry;
    return 0;
}

/** Returns true if an entry at the given index was found and removed, else false */
-(bool)removeEntryAtIndex:(uint32_t)index
{
    for (int i = 0; i < _currCount; i++) {
        if (self->elements[i].indexInDir == index) {
            /*
             * Found our entry, need to remove it. We don't want holes in our array
             * so copy in the last element.
             */
            if (i != _currCount - 1) {
                self->elements[i].hash = self->elements[_currCount - 1].hash;
                self->elements[i].indexInDir = self->elements[_currCount - 1].indexInDir;
            }
            _currCount--;
            return true;
        }
    }
    return false;
}

@end

@interface DirNameCache()

@property (atomic) uint32_t numHashBuckets;
@property (atomic) uint32_t numHashValues;

@end


@implementation DirNameCache

-(instancetype _Nullable)initWithDirEntrySize:(uint32_t)dirEntrySize
{
    self = [super init];
    if (self) {
        _nameCacheBuckets = [[NSMutableDictionary alloc] initWithCapacity:MAX_NUM_BUCKETS];
        _dirEntrySize = dirEntrySize;
    }
    return self;
}

-(uint32_t)hash:(struct unistr255 *)theName
{
    return (uint32_t)crc32(0, (const Bytef*)theName->chars, ((unsigned int)theName->length * sizeof(theName->chars[0])));
}

-(NSError * _Nullable)insertDirEntryNamed:(char *)chrString
                                 ofLength:(size_t)stringLen
                              offsetInDir:(uint64_t)offset
{
    struct unistr255 unistrName = {0};
    int error = CONV_UTF8ToUnistr255((uint8_t *)chrString,
                                     stringLen, &unistrName, UTF_SFM_CONVERSIONS);
    if (error) {
        return fs_errorForPOSIXError(error);
    }
    return [self insertDirEntryNamedUtf16:&unistrName offsetInDir:offset];
}

-(void)removeDirEntryNamed:(char *)chrString
                  ofLength:(size_t)stringLen
               offsetInDir:(uint64_t)offset
{
    struct unistr255 unistrName = {0};
    int error = CONV_UTF8ToUnistr255((uint8_t *)chrString,
                                     stringLen, &unistrName, UTF_SFM_CONVERSIONS);
    if (!error) {

        [self removeDirEntryNamedUtf16:&unistrName offsetInDir:offset];
    }
}

-(void)lookupDirEntryNamed:(char *)chrString
                  ofLength:(size_t)stringLen
              replyHandler:(void(^)(NSError * _Nullable error, uint64_t entryOffsetInDir)) reply
{
    struct unistr255 unistrName = {0};

    int error = CONV_UTF8ToUnistr255((uint8_t *)chrString,
                                     stringLen, &unistrName, UTF_SFM_CONVERSIONS);
    if (error) {
        return reply(fs_errorForPOSIXError(error), 0);
    }
    return [self lookupDirEntryNamedUtf16:&unistrName
                             replyHandler:reply];
}

-(NSError * _Nullable)insertDirEntryNamedUtf16:(struct unistr255 *)name
                                   offsetInDir:(uint64_t)offset
{
    NameCacheEntry newEntry = {0};
    uint32_t indexInDir = (uint32_t)(offset / _dirEntrySize);
    NameCacheBucket *currentBucket = NULL;
    NSString *bucketKey = nil;
    int err = ENOMEM;

    /* Check we're not exceeding capacity */
    if (_numHashValues == MAX_HASH_VALUES) {
        os_log_debug(fskit_std_log(), "%s: Name cache full (%u/%u)", __func__, _numHashValues, MAX_HASH_VALUES);
        err = ENOSPC;
        goto out_err;
    }
    CONV_Unistr255ToLowerCase(name);
    newEntry.hash = [self hash:name];
    newEntry.indexInDir = indexInDir;
    char cString[5];
    snprintf(cString, 5, "%u", (newEntry.hash % MAX_NUM_BUCKETS));
    bucketKey = [[NSString alloc] initWithUTF8String:cString];

    /* Check if there's already a bucket at that index */
    currentBucket = [_nameCacheBuckets objectForKey:bucketKey];
    if (currentBucket == nil) {
        /* Need to create a new bucket */
        currentBucket = [[NameCacheBucket alloc] init];
        [_nameCacheBuckets setObject:currentBucket forKey:bucketKey];
        _numHashBuckets++;
    } else {
        /* Make sure the offset is not already present */
        for (int i = 0; i < currentBucket.currCount; i++) {
            if (currentBucket->elements[i].indexInDir == indexInDir) {
                os_log_fault(fskit_std_log(), "%s: Dir index %u is already in the cache", __func__, indexInDir);
                err = EINVAL;
                goto out_err;
            }
        }
    }

    /* Add the new entry */
    if ([currentBucket addEntry:newEntry]) {
        os_log_error(fskit_std_log(), "%s: Failed to add to bucket", __func__);
        goto out_err;
    }
    _numHashValues++;

    return nil;
out_err:
    return fs_errorForPOSIXError(err);
}

-(void)removeDirEntryNamedUtf16:(struct unistr255 *)name
                    offsetInDir:(uint64_t)offset
{
    uint32_t indexInDir = (uint32_t)(offset / _dirEntrySize);
    NameCacheBucket *currentBucket = NULL;
    NSString *bucketKey = nil;
    uint32_t entryHash = 0;

    if (_numHashValues == 0) {
        return; /* Name cache is empty */
    }
    CONV_Unistr255ToLowerCase(name);
    /* Calculate the hash key */
    entryHash = [self hash:name];
    char cString[5];
    snprintf(cString, 5, "%u", (entryHash % MAX_NUM_BUCKETS));
    bucketKey = [[NSString alloc] initWithUTF8String:cString];

    currentBucket = [_nameCacheBuckets objectForKey:bucketKey];
    if (!currentBucket) {
        /* Nothing in the hash entry, return */
        return;
    }

    /* Remove the entry from the bucket if it's there */
    if ([currentBucket removeEntryAtIndex:indexInDir]) {
        _numHashValues--;
    }

    /* If the bucket is now empty, need to remove it as well */
    if (currentBucket.currCount == 0) {
        free(currentBucket->elements);
        currentBucket->elements = NULL;
        [_nameCacheBuckets removeObjectForKey:bucketKey];
        _numHashBuckets--;
    }
}

-(void)lookupDirEntryNamedUtf16:(struct unistr255 *)name
                   replyHandler:(void(^)(NSError * _Nullable error, uint64_t entryOffsetInDir)) reply
{
    NameCacheBucket *currentBucket = NULL;
    NSString *bucketKey = nil;
    uint32_t entryHash = 0;

    if (_numHashValues == 0 || _numHashBuckets == 0) {
        return reply(fs_errorForPOSIXError(ENOENT), 0);
    }

    CONV_Unistr255ToLowerCase(name);
    /* Calculate the hash key */
    entryHash = [self hash:name];
    char cString[5];
    snprintf(cString, 5, "%u", (entryHash % MAX_NUM_BUCKETS));
    bucketKey = [[NSString alloc] initWithUTF8String:cString];

    currentBucket = [_nameCacheBuckets objectForKey:bucketKey];
    if (currentBucket == nil) {
        /* The entry is not in the cache */
        return reply(fs_errorForPOSIXError(ENOENT), 0);
    }

    /* Bucket exists, find the first entry with the same hash */
    for (int i = 0; i < currentBucket.currCount; i++) {
        if (currentBucket->elements[i].hash == entryHash) {
            return reply(nil, (uint64_t)currentBucket->elements[i].indexInDir * _dirEntrySize);
        }
    }
    return reply(fs_errorForPOSIXError(ENOENT), 0);
}

@end


@interface DirNameCachePool ()

@property NSMutableArray<DNCPoolEntry*> *pool;
@property dispatch_semaphore_t poolSemaphore;
@property uint32_t capacity;

@end


@interface DNCPoolEntry()

@property uint32_t cacheKey;
@property DirNameCache *dnc;
@property NSDate *timestamp;

-(instancetype)initWithDNC:(DirNameCache*)dnc
                  cacheKey:(uint32_t)key;

@end

@implementation DNCPoolEntry

-(instancetype)initWithDNC:(DirNameCache*)dnc
                  cacheKey:(uint32_t)key
{
    self = [super init];
    if (self) {
        _dnc = dnc;
        _cacheKey = key;
    }
    return self;
}

@end

@implementation DirNameCachePool

- (instancetype)init
{
    self = [super init];
    if (self) {
        _capacity = DIR_NAME_CACHES_LIMIT;
        _pool = [[NSMutableArray alloc] initWithCapacity:DIR_NAME_CACHES_LIMIT];
        self.poolSemaphore = dispatch_semaphore_create(DIR_NAME_CACHES_LIMIT);
    }
    return self;
}

-(DNCPoolEntry *)getDNCEntryByKey:(uint32_t)key
{
    for (DNCPoolEntry *entry in _pool) {
        if (entry.cacheKey == key) {
            return entry;
        }
    }
    return nil;
}

/* Returns the least recently used entry in the pool which is also not in use. */
-(DNCPoolEntry *)getAvailableEntry
{
    DNCPoolEntry *entryToRemove = nil;

    for (DNCPoolEntry *entry in _pool) {
        if (entry.dnc.isInUse) {
            /* In use by someone, don't touch it */
            continue;
        }
        if (!entryToRemove || entry.timestamp < entryToRemove.timestamp) {
            entryToRemove = entry;
        }
    }

    if (!entryToRemove) {
        os_log_error(fskit_std_log(), "%s: No free entries", __func__);
    }

    return entryToRemove;
}

- (void)getNameCacheForDir:(nonnull DirItem *)dir
                cachedOnly:(bool)cachedOnly
              replyHandler:(void(^)(NSError * _Nullable error,
                                    DirNameCache * _Nullable cache,
                                    bool isNew))reply;
{
    /* The pool has <size> available slots, we need to hold onto one of them. */
    if (dispatch_semaphore_wait(_poolSemaphore, DISPATCH_TIME_NOW)) {
        os_log_debug(fskit_std_log(), "%s: dispatch_semaphore_wait timed out", __func__);
        return reply(nil, nil, false);
    }

    @synchronized (_pool) {
        DNCPoolEntry *entry = nil;
        DirNameCache *dnc = nil;

        /* Getting here means we hold onto one of the pool slots */
        entry = [self getDNCEntryByKey:dir.firstCluster];
        if (entry == nil) {
            /* No such DNC cached */
            if (cachedOnly) {
                /* Must release the semaphore as we're not using the cache */
                dispatch_semaphore_signal(_poolSemaphore);
                return reply(nil, nil, false); // TODO: Maybe some return value?
            }
            /* Find a free cache slot to use or we should free one */
            if (_pool.count < _capacity) {
                /* We have a free slot */
                dnc = [[DirNameCache alloc] initWithDirEntrySize:[dir dirEntrySize]];
                dnc.isInUse = true; /* Will be cleared when doneWithNameCacheForDir:reply is called */
                entry = [[DNCPoolEntry alloc] initWithDNC:dnc cacheKey:dir.firstCluster];
                entry.timestamp = [NSDate now];
                [_pool addObject:entry];
            } else {
                /* Need to find the LRU one which is not in use */
                entry = [self getAvailableEntry];
                if (entry == nil) {
                    os_log_fault(fskit_std_log(), "%s: No available entry", __func__);
                    dispatch_semaphore_signal(_poolSemaphore);
                    return reply(fs_errorForPOSIXError(EFAULT), nil, false);
                }
                /* We have an entry we can use */
                entry.cacheKey = dir.firstCluster;
                entry.timestamp = [NSDate now];
                entry.dnc = dnc = [[DirNameCache alloc] initWithDirEntrySize:[dir dirEntrySize]];
                entry.dnc.isInUse = true;
            }
            return reply(nil, entry.dnc, true);
        }
        /* Getting here means we found a DNC for our directory */
        if (entry.dnc.isInUse) {
            /* This shouldn't happen! Signal the semaphore and report an error. */
            dispatch_semaphore_signal(_poolSemaphore);
            os_log_fault(fskit_std_log(), "%s: DNC for current dir is in use (%u)", __func__, dir.firstCluster);
            return reply(fs_errorForPOSIXError(EFAULT), nil, false);
        }
        entry.dnc.isInUse = true; /* Will be cleared when doneWithNameCacheForDir:reply is called */
        entry.timestamp = [NSDate now];
        return reply(nil, entry.dnc, false);
    }
}

-(void)removeNameCacheForDir:(nonnull DirItem *)dir
{
    @synchronized (_pool) {
        DNCPoolEntry *entryToRemove = nil;
        for (DNCPoolEntry *entry in _pool) {
            if (entry.cacheKey == dir.firstCluster) {
                entryToRemove = entry; // Can't modify while enumerating
            }
        }
        if (entryToRemove) {
            [_pool removeObject:entryToRemove];
        }
    }
}

- (void)doneWithNameCacheForDir:(nonnull DirItem *)dir
{
    @synchronized (_pool) {
        DNCPoolEntry *entry = [self getDNCEntryByKey:dir.firstCluster];

        if (!entry) {
            os_log_error(fskit_std_log(), "%s: Entry for key %u not found", __func__, dir.firstCluster);
            return;
        }
        if (!entry.dnc.isInUse) {
            /* This shouldn't have happened */
            os_log_error(fskit_std_log(), "%s: Entry for key %u is already set as not in use", __func__, dir.firstCluster);
        }
        entry.dnc.isInUse = false;
        dispatch_semaphore_signal(_poolSemaphore);
    }
}

-(void)check
{
    for (DNCPoolEntry *entry in _pool) {
        os_log(fskit_std_log(), "Key %d, timestamp %@", entry.cacheKey, entry.timestamp);
    }
}

@end

NS_ASSUME_NONNULL_END
