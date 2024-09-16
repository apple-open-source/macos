/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef DirItem_h
#define DirItem_h

#import <Foundation/Foundation.h>
#import "FATItem.h"

NS_ASSUME_NONNULL_BEGIN

@protocol DirOps <NSObject>

/** Returns the dir block size that should be used for thie directory. */
-(uint64_t)getDirBlockSize;

/** Returns the directory size. */
-(uint64_t)getDirSize;

/** Returns the directory entry struct size. */
-(uint32_t)dirEntrySize;

/**
 * Create '.' and '..' dir entries for a newly created directory.
 * Sub-classes should implement this method in case they need to create '.' and '..' dir entries when we create new directories.
 */
-(NSError *)createDotEntriesWithAttrs:(FSItemAttributes *)mkdirAttrs;

/**
 * Update the modification time of the directory.
 * Sub-classes should implement this method in case they need to update the
 * modification time of the directory when we create/remove items in it.
 */
-(NSError *)updateModificationTimeOnCreateRemove;

/**
 * Update A/M/B times of the dot ('.') dir entry if so required by the given attributes.
 */
-(NSError *)updateDotDirEntryTimes:(FSItemSetAttributesRequest *)attrs;
/**
 Directory iterator.
 @param startOffsetInDir offset from which to start iterating.
 @param options special options to apply when iterating.
 @param reply the reply block returns iterateDirStatus, which tells the iterator whether it should continue or stop iterating the dir.
    In case of a failure, the reply block is called with a non-nil error and zeros/nils, and its return value is ignored.
    - result is the type of the found dir entry.
    - dirEntryOffsetInVolume is the entry's offset in the volume.
    - dirEntryOffset is the entry offset in the directory.
    - name is the entry's name.
    - dirEntryData is the entry's data.
 */
-(void)iterateFromOffset:(uint64_t)startOffsetInDir
                 options:(iterateDirOptions)options
            replyHandler:(iterateDirStatus (^)(NSError *error,
                                               dirEntryType result,
                                               uint64_t dirEntryOffset,
                                               struct unistr255 * _Nullable name,
                                               DirEntryData * _Nullable dirEntryData))reply;

/**
 Create new directory entry-set for a new item.
 @param unistrName new item name in UTF-16.
 @param itemType new item type
 @param firstCluster first cluster of the new item
 @param attrs new item attributes
 @param offsetInDir offset in dir for the new entry-set
 @param hidden true if the file is hidden, false o.w
 @param reply In case of success, calls reply with error = nil and the created dirEntryData object.
    In case of an error, calls reply with error != nil, and dirEntryData = nil.
 */
-(void)createEntrySetForName:(struct unistr255)unistrName
                    itemType:(FSItemType)itemType
                firstCluster:(uint32_t)firstCluster
                       attrs:(FSItemAttributes *)attrs
                 offsetInDir:(uint64_t)offsetInDir
                      hidden:(bool)hidden
                replyHandler:(void (^)(NSError * _Nullable error,
                                       DirEntryData * _Nullable dirEntryData))reply;

/**
 Create and write directory entries to disk, based on the given data and name, at the given offset in dir.
 @param dirEntryData dir entry data to write and/or generate the name entries from.
 @param startOffsetInDir the offset in dir to which we should start writing.
 @param name item name to generate the name entries from.
 @param numberOfEntries number of entries to write.
 */
-(NSError *)writeDirEntriesToDisk:(DirEntryData *)dirEntryData
                         atOffset:(uint64_t)startOffsetInDir
                             name:(struct unistr255)name
                  numberOfEntries:(uint32_t)numberOfEntries;

/**
 Write the given dir entry data to disk.
 @param dirEntryData dir entry data to write
 */
-(NSError *)writeDirEntryDataToDisk:(DirEntryData *)dirEntryData;

/**
 Mark directory entry-set as deleted for a new item.
 @param forItem item to mark as seleted in the directory
 */
-(NSError *)markDirEntriesAsDeleted:(FATItem *)forItem;

-(NSError *)verifyCookieOffset:(uint32_t)offset;

@end


@interface DirItem : FATItem <DirOps>

@property bool isRoot;
@property uint64_t dirVersion;
@property DirNameCache * _Nullable dirNameCache;
@property uint32_t tableBusyCount;
@property uint32_t maxRADirBlock;
@property uint64_t offsetForNewEntry;
@property dispatch_semaphore_t sem;

/*
 * TODO: we have ifdef MSDOS/EXFAT_NLINK_IS_CHILD_COUNT which aren't defined anywhere.
 * What do we want to do about them?
 */

/** Returns true in case the volume is FAT12/FAT16, and this is the root directory. */
-(bool)isFat1216RootDir;

/**
 Lookup a specific dir entry in the directory
 @param lookupName dir entry name to lookup
 @param nameCache dir name cache to lookup in. If nil, don't lookup in cache.
 @param lookupOffset optional hint to where to look for the dir entry.
 @param reply In case of a failure returns a non-nil error and dirEntryData = nil. Else, error is nil and the relevant dir entry data is returned.
 */
-(void)lookupDirEntryNamed:(NSString *)lookupName
              dirNameCache:(DirNameCache * _Nullable)nameCache
              lookupOffset:(uint64_t * _Nullable)lookupOffset
              replyHandler:(void (^)(NSError *error,
                                     DirEntryData *dirEntryData))reply;

/** Fills the given name cache with all dir entries of the directory. */
-(NSError * _Nullable)fillNameCache:(DirNameCache *)nameCache;

/**
 Create a new directory entry-set for a new item, look for available slots in dir to place it, and write it to disk.
 @param name new item name
 @param type new item type
 @param attrs new item attributes
 @param firstDataCluster first cluster of the new item
 @param reply In case of success, calls reply with error = nil, dirEntryOffsetInVolume = first dir entry offset in the volume, offsetInDir = the starting offset in dir of the newly created dir entries.
    In case of an error, calls reply with error != nil, dirEntryOffsetInVolume = 0 and offsetInDir = 0.
 */
-(void)createNewDirEntryNamed:(NSString *)name
                         type:(FSItemType)type
                   attributes:(FSItemAttributes *)attrs
             firstDataCluster:(uint32_t)firstDataCluster
                 replyHandler:(void (^)(NSError * _Nullable error,
                                        uint64_t offsetInDir))reply;

/**
 Iterate over dir entries and write their updated contents to disk if requested.
 @param startOffsetInDir offset in dir to start iterating from.
 @param numberOfEntries number of dir entries to iterate over.
 @param writeToDisk whether or not to write the updated dir entries to disk.
 @param reply in case of success, called for each dir entry we're iterating over.
    In case of an error, calls reply with error != nil, and dirEntry = nil.
 */
-(void)iterateDirEntriesAtOffset:(uint64_t)startOffsetInDir
                      numEntries:(uint32_t)numberOfEntries
               shouldWriteToDisk:(bool)writeToDisk
                    replyHandler:(void (^)(NSError * _Nullable error, void * _Nullable dirEntry))reply;

/**
 Mark directory entry-set as deleted for a new item and update the directory modify time.
 Returns NSError when failed to mark the directory entries as deleted.
 @param forItem item to mark as seleted in the directory
 */
-(NSError *)markDirEntriesAsDeletedAndUpdateMtime:(FATItem *)forItem;

/** Return NSError if the directory is not empty or if could not complete the check. o.w return nil */
-(NSError *)checkIfEmpty;

/**
 Get the start offset of the dir entry for given index in the directory.
 @param index index number starting from zero.
 @param reply in case of success, called with nil error, offset of the dir entry and reachedEOF == false.
    In case of an error, calls reply with error != nil. In case the index >= number of entries found in the directory,
    called with error == nil, reachedEOF == true.
 */
-(void)getDirEntryOffsetByIndex:(uint32_t)index
                   replyHandler:(void (^)(NSError * _Nullable error, uint64_t offset, bool reachedEOF))reply;

@end

NS_ASSUME_NONNULL_END

#endif /* DirItem_h */
