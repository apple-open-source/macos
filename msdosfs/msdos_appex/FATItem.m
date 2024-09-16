/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "ExtensionCommon.h"
#import "DirNameCache.h"
#import "FATManager.h"
#import "FATVolume.h"
#import "FATItem.h"
#import "DirItem.h"

@implementation DirEntryData

+(instancetype)dynamicCast:(id)candidate
{
	return ([candidate isKindOfClass:self]) ? candidate : nil;
}

-(instancetype)initWithData:(NSData *)data;
{
    return nil; // sub-classes should implement.
}

-(void)getAccessTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)setAccessTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)getModifyTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)setModifyTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)getChangeTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)setChangeTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)getBirthTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(void)setBirthTime:(struct timespec *)tp
{
    return; // sub-classes should implement.
}

-(FSItemType)type
{
    return FSItemTypeUnknown; // sub-classes should implement.
}

-(uint32_t)bsdFlags
{
    return 0; // sub-classes should implement.
}

-(NSError *)setBsdFlags:(uint32_t)newFlags
{
    return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

-(uint32_t)getFirstCluster:(FileSystemInfo *)systemInfo
{
    return 0; // sub-classes should implement.
}

-(void)setFirstCluster:(uint32_t)firstCluster
		fileSystemInfo:(FileSystemInfo *)systemInfo
{
	return; // sub-classes should implement.
}

-(uint8_t *)getName
{
    return NULL; // sub-classes should implement.
}

-(void)setName:(uint8_t *)name
{
    return; // sub-classes should implement.
}

-(uint64_t)getSize
{
    return 0; // sub-classes should implement.
}

-(void)setSize:(uint64_t)size
{
    return; // sub-classes should implement.
}

-(uint64_t)getValidDataLength
{
    return 0; // sub-classes should implement.
}

-(void)setValidDataLength:(uint64_t)validDataLength
{
    return; // sub-classes should implement.
}
-(void)setArchiveBit
{
    return; // sub-classes should implement.
}

-(uint64_t)calcFirstEntryOffsetInVolume:(FileSystemInfo *)systemInfo
{
    return 0; //sub-classes should implement.
}

@end


@implementation FATItem

+(instancetype)dynamicCast:(id)candidate
{
    return ([candidate isKindOfClass:self]) ? candidate : nil;
}

- (instancetype)initInVolume:(FATVolume  * _Nonnull )volume
                       inDir:(FATItem * _Nullable)parentDir
                  startingAt:(uint32_t)firstCluster
                    withData:(DirEntryData * _Nullable)entryData
                     andName:(nonnull NSString *)name
                      isRoot:(bool)isRoot;
{
    __block NSError *error = nil;
    __block uint32_t numberOfClusters = 0;
    __block uint32_t lastCluster = 0;

    self = [super init];
    if (self) {
        _volume = volume;
        _firstCluster = firstCluster;
        _entryData = entryData;
        _name = name;
        _parentDir = parentDir;
        _isDeleted = false;

        if (_firstCluster > 0) {
            /* Figure out the numberOfClusters and lastCluster. */
            [_volume.fatManager clusterChainLength:self
                                      replyHandler:^(NSError *clusterChainLengthError,
                                                     uint32_t lastClusterOfItem,
                                                     uint32_t length) {
                if (clusterChainLengthError) {
                    error = clusterChainLengthError;
                } else {
                    numberOfClusters = length;
                    lastCluster = lastClusterOfItem;
                }
            }];
            if (error) {
                os_log_error(fskit_std_log(), "%s: couldn't get cluster chain length. Error = %@.", __FUNCTION__, error);
                return nil;
            }
            _numberOfClusters = numberOfClusters;
            _lastCluster = lastCluster;
        }
    }
    return self;
}

-(uint64_t)getFileID
{
    return [self.volume getFileID:self.entryData];
}

-(void)setDeleted
{
    if (self.isDeleted == false) {
        /* Consider multiple FDs for the same file */
        @synchronized (self.volume.openUnlinkedFiles) {
            uint64_t curVal = [self.volume.openUnlinkedFiles objectAtIndex:0].unsignedLongLongValue;
            [self.volume.openUnlinkedFiles replaceObjectAtIndex:0
                                                     withObject:[NSNumber numberWithUnsignedLongLong:(curVal + 1)]];
        }
        self.isDeleted = true;
    }
}

/* Default setter for firstCluster property */
-(void)setFirstCluster:(uint32_t)firstCluster
{
    _firstCluster = firstCluster;
    [self.entryData setFirstCluster:firstCluster
                     fileSystemInfo:self.volume.systemInfo];
}

-(FSItemAttributes *)getAttributes:(nonnull FSItemGetAttributesRequest *)desired
{
    FSItemAttributes *attrs = [[FSItemAttributes alloc] init];
    struct timespec timespec = {0};

    if ([desired isWanted:FSItemAttributeType]) {
        attrs.type = (self.entryData) ? self.entryData.type : FSItemTypeUnknown;
    }

    if ([desired isWanted:FSItemAttributeMode]) {
        attrs.mode = (0000007 << 6); // (LI_FA_MODE_RWX) TBD
    }

    if ([desired isWanted:FSItemAttributeLinkCount]) {
        attrs.linkCount = 1;
    }

    uint64_t allocSize = (uint64_t)self.numberOfClusters * (uint64_t)self.volume.systemInfo.bytesPerCluster;
    if ([desired isWanted:FSItemAttributeAllocSize]) {
        // This is true for most of the FAT item. In case the allocSize should be something else
        // for specific items, that item will have to override getAttributes and do what it should
        attrs.allocSize = allocSize;
    }

    if ((self.entryData) && ([desired isWanted:FSItemAttributeSize])) {
        if (self.entryData.type == FSItemTypeDirectory) {
            attrs.size = allocSize;
        } else if (self.entryData.type == FSItemTypeFile) {
            attrs.size = [self.entryData getValidDataLength];
        }
    }

    if ([desired isWanted:FSItemAttributeFileID]) {
        attrs.fileID = [self getFileID];
    }

    if ([desired isWanted:FSItemAttributeParentID]) {
        if (self.parentDir) {
            attrs.parentID = [self.parentDir getFileID];
        } else {
            DirItem *dirItem = [DirItem dynamicCast:self];
            if (dirItem) {
                if (dirItem.isRoot) {
                    /* The root dir doesn't have a parent, so we use a random fileID. */
                    attrs.parentID = [self.volume getNextAvailableFileID];
                } else {
                    os_log_fault(fskit_std_log(), "%s: Failed to get parent id (dir item).", __FUNCTION__);
                }
            } else {
                os_log_fault(fskit_std_log(), "%s: Failed to get parent id (item is not a dir)", __FUNCTION__);
            }
        }
    }

    if ([desired isWanted:FSItemAttributeFlags]) {
        attrs.flags = self.entryData.bsdFlags;
    }

    attrs.inhibitKOIO = false;

    if (([desired isWanted:FSItemAttributeAccessTime]) && (self.entryData)) {
        [self.entryData getAccessTime:&timespec];
        attrs.accessTime = timespec;
    }

    if (([desired isWanted:FSItemAttributeModifyTime]) && (self.entryData)) {
        [self.entryData getModifyTime:&timespec];
        attrs.modifyTime = timespec;
    }

    if (([desired isWanted:FSItemAttributeChangeTime]) && (self.entryData)) {
        [self.entryData getChangeTime:&timespec];
        attrs.changeTime = timespec;
    }

    if (([desired isWanted:FSItemAttributeBirthTime]) && (self.entryData)) {
        [self.entryData getBirthTime:&timespec];
        attrs.birthTime = timespec;
    }

    return attrs;
}

/*
 * No need to update modify time after updating the dir entry in two cases:
 * 1. We were already asked to update it.
 * 2. Only Access time was modified.
 */
-(bool)shouldUpdateMTimeInSetAttr:(FSItemSetAttributesRequest *)req
{
    // We only want to update the mTime in case we have truncated the file,
    // and haven't been asked explicitly to update the mTime.
    return ([req isValid:FSItemAttributeSize] && ![req isValid:FSItemAttributeModifyTime]);
}


-(NSError *)setAttributes:(nonnull FSItemSetAttributesRequest *)newAttributes
{
    DirItem *theDirItem = [DirItem dynamicCast:self];
    DirEntryData *entryData = self.entryData;
    FSItemAttribute consumedAttributes = 0;
    struct timespec timeSpec = {0};
    NSError *error = nil;
    bool modified = false;

    if (theDirItem && theDirItem.isRoot && theDirItem.entryData == nil) {
        /* No volume entry, ignore */
        return nil;
    }

    [entryData setArchiveBit];

    if ([newAttributes isValid:FSItemAttributeSize]) {
        /* Only for files */
        FileItem *theFileItem = [FileItem dynamicCast:self];
        if (!theFileItem) {
            return (fs_errorForPOSIXError(EPERM));
        }

        uint64_t allocSize = self.numberOfClusters * self.volume.systemInfo.bytesPerCluster;
        if (newAttributes.size > allocSize || newAttributes.size < [entryData getSize]) {
            /*
             * 1. In case newAttributes.size > allocSize, we need to allocate clusters.
             * 2. We should free clusters only if we've been asked to truncate below EOF.
             *    In case EOF < newAttributes.size < allocSize, we will just update the
             *    size field below, as these extents may be mapped by upper layers,
             *    so we can't free them.
             */
            error = [theFileItem truncateTo:newAttributes.size
                               allowPartial:false
                               mustBeContig:false];
            if (error) {
                os_log_error(fskit_std_log(), "%s: Failed to truncate to %llu", __func__, newAttributes.size);
                return (error);
            }
        }

        /* Update file size */
        [entryData setSize:newAttributes.size];
        /* Set valid data length */
        [entryData setValidDataLength:newAttributes.size];

        /* Check if all preallocated clusters are used */
        if (theFileItem.isPreAllocated) {
            if (roundup([entryData getSize], self.volume.systemInfo.bytesPerCluster) / self.volume.systemInfo.bytesPerCluster == theFileItem.numberOfClusters) {
                [theFileItem setPreAllocated:false];
            }
        }
        [newAttributes wasConsumed:FSItemAttributeSize];
        modified = true;
    }

    /* If the item was unlinked, no need to update direntry */
    if (!self.isDeleted && theDirItem != nil && !theDirItem.isRoot) {
        if ([newAttributes isValid:FSItemAttributeAccessTime] ||
            [newAttributes isValid:FSItemAttributeModifyTime] ||
            [newAttributes isValid:FSItemAttributeBirthTime]) {
            error = [theDirItem updateDotDirEntryTimes:newAttributes];
            if (error) {
                return error;
            }
        }
    }

    if ([newAttributes isValid:FSItemAttributeAccessTime]) {
        timeSpec = newAttributes.accessTime;
        [entryData setAccessTime:&timeSpec];
        consumedAttributes |= FSItemAttributeAccessTime;
        modified = true;
    }

    if ([newAttributes isValid:FSItemAttributeModifyTime]) {
        timeSpec = newAttributes.modifyTime;
        [entryData setModifyTime:&timeSpec];
        consumedAttributes |= FSItemAttributeModifyTime;
        modified = true;
    }

    if ([newAttributes isValid:FSItemAttributeBirthTime]) {
        timeSpec = newAttributes.birthTime;
        [entryData setBirthTime:&timeSpec];
        consumedAttributes |= FSItemAttributeBirthTime;
        modified = true;
    }

    if ([newAttributes isValid:FSItemAttributeFlags]) {
        error = [entryData setBsdFlags:newAttributes.flags];
        modified = true;
    }

    newAttributes.consumedAttributes = consumedAttributes;
    if ([self shouldUpdateMTimeInSetAttr:newAttributes] && modified) {
        clock_gettime(CLOCK_REALTIME, &timeSpec);
        [entryData setModifyTime:&timeSpec];
    }

    /* Flush the entry to disk */
    [self flushDirEntryData];

    return error;
}

-(NSError *)flushDirEntryData
{
    DirItem *dirItem = [DirItem dynamicCast:self];
    DirItem *dirItemToUpdate = nil;

    if (self.isDeleted) {
        /* No need to flush an unlinked item */
        return nil;
    }

    dirItemToUpdate = [DirItem dynamicCast:self.parentDir];

    if (dirItemToUpdate == nil) {
        /* Only the root dir doesn't have a parent dir */
        if ([dirItem isRoot]) {
            dirItemToUpdate = dirItem;
        } else {
            os_log_error(fskit_std_log(), "%s: No parent dir!", __func__);
            return fs_errorForPOSIXError(EINVAL);
        }
    }

    return [dirItemToUpdate writeDirEntryDataToDisk:self.entryData];
}


-(NSError *)reclaim
{
    SymLinkItem *symLinkItem = [SymLinkItem dynamicCast:self];
    FileItem *fileItem = nil;
    DirItem *dirItem = nil;

    if ([self isDeleted]) {
        if (_numberOfClusters > 0) {
            if (symLinkItem != nil) {
                /* Dir's block are purged during remove() */
                [symLinkItem purgeMetaBlocksFromCache:^(NSError * _Nullable error) {
                    if (error) {
                        os_log_error(fskit_std_log(), "%s: Couldn't purge symlink's meta blocks. Error: %@", __func__, error);
                    }
                }];
            }
            [self.volume.fatManager setDirtyBitValue:dirtyBitDirty
                                        replyHandler:^(NSError * _Nullable fatError) {
                if (fatError) {
                    /* Log the error, keep going */
                    os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __func__, fatError);
                }
            }];
            [self.volume.fatManager freeClusters:self.numberOfClusters
                                          ofItem:self
                                    replyHandler:^(NSError * _Nullable error) {
                if (error) {
                    os_log_error(fskit_std_log(), "%s: Failed to free clusters, error %@", __func__, error);
                }
            }];
        }
        @synchronized (self.volume.openUnlinkedFiles) {
            uint64_t curVal = [self.volume.openUnlinkedFiles objectAtIndex:0].unsignedLongLongValue;
            if (curVal == 0) {
                os_log_error(fskit_std_log(), "%s: Expected number of open-unlinked files to be > 0", __FUNCTION__);
            } else {
                [self.volume.openUnlinkedFiles replaceObjectAtIndex:0
                                                         withObject:[NSNumber numberWithUnsignedLongLong:(curVal - 1)]];
            }
        }
    }

    fileItem = [FileItem dynamicCast:self];
    if (fileItem && fileItem.isPreAllocated) {
        uint64_t usedClusters = roundup([fileItem.entryData getSize], self.volume.systemInfo.bytesPerCluster) / self.volume.systemInfo.bytesPerCluster;
        uint64_t allocatedClusters = fileItem.numberOfClusters;

        if (allocatedClusters > usedClusters) {
            [self.volume.fatManager setDirtyBitValue:dirtyBitDirty
                                        replyHandler:^(NSError * _Nullable fatError) {
                if (fatError) {
                    /* Log the error, keep going */
                    os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __func__, fatError);
                }
            }];
            [self.volume.fatManager freeClusters:(uint32_t)(allocatedClusters - usedClusters)
                                          ofItem:fileItem
                                    replyHandler:^(NSError * _Nullable error) {
                if (error) {
                    os_log_error(fskit_std_log(), "%s: Failed to free preallocated clusters, error %d", __FUNCTION__, (int)error.code);
                }
            }];
            [fileItem setPreAllocated:false];
        }
    }

    dirItem = [DirItem dynamicCast:self];
    if (dirItem) {
        /*
         * If this is a dir, free its dir name cache if exists.
         * We're not expected to hold a pool slot at this point, so don't
         * attempt to free it.
         */
        [self.volume.nameCachePool removeNameCacheForDir:dirItem];
    }
    return nil;
}

/*
 * Implemented separately for symlinks and directories, this method is not
 * supported for file items and shouldn't be called for them.
 */
-(void)purgeMetaBlocksFromCache:(void(^)(NSError * _Nullable))reply
{
    if ([FileItem dynamicCast:self]) {
        os_log_error(fskit_std_log(), "%s: Should not be called for a file item", __func__);
    }
    return reply(fs_errorForPOSIXError(ENOTSUP));
}

@end


@implementation SymLinkItem

- (instancetype)initInVolume:(FATVolume *)volume
                       inDir:(FATItem * _Nullable)parentDir
                  startingAt:(uint32_t)firstCluster
                    withData:(DirEntryData * _Nullable)entryData
                     andName:(nonnull NSString *)name
{
    // TODO add support here or in subclass for reading the length from disk etc.
    self = [super initInVolume:volume
                         inDir:parentDir
                    startingAt:firstCluster
                      withData:entryData
                       andName:name
                        isRoot:false];
    return self;
}

+(NSError *)createSymlinkFromContent:(nonnull NSData *)contents
                            inBuffer:(nonnull NSMutableData *)buffer
{
    NSString *linkStr = [[NSString alloc] initWithData:contents encoding:NSUTF8StringEncoding];

    if (linkStr == nil) {
        return fs_errorForPOSIXError(EINVAL);
    }

    struct symlink* linkBytes = (struct symlink*)buffer.mutableBytes;

    // Set link magic
    memcpy(linkBytes->magic, symlink_magic, DOS_SYMLINK_MAGIC_LENGTH);

    // Set Link Length
    uint32_t length = (uint32_t)strlen(linkStr.UTF8String);
    char* linkLengthBytes = linkBytes->length;
    snprintf(linkLengthBytes, 5, "%04u\n", length);

    NSData *digest = [Utilities getMD5Digest:32
                                     forData:linkStr.UTF8String
                                      length:length];

    memcpy(linkBytes->md5, digest.bytes, 32);

    linkBytes->newline2 = '\n';

    // Set the data into the link
    memcpy(linkBytes->link, linkStr.UTF8String, length);

    /* Add a newline if there is room */
    if (length < SYMLINK_LINK_MAX) {
        linkBytes->link[length] = '\n';
    }

    /* Pad with spaces if there is room */
    if ((length + 1) < SYMLINK_LINK_MAX) {
        memset(&linkBytes->link[length + 1], ' ', (SYMLINK_LINK_MAX - (length + 1)));
    }

    return nil;
}

+(void)verifyAndGetLink:(NSMutableData *)linkData
           replyHandler:(nonnull void (^)(NSError * _Nullable, NSString * _Nullable linkStr))reply
{
    struct symlink *symlinkBytes = (struct symlink *)linkData.mutableBytes;
    // Verify the magic
    if (strncmp(symlinkBytes->magic, symlink_magic, DOS_SYMLINK_MAGIC_LENGTH) != 0) {
        return reply(fs_errorForPOSIXError(EINVAL), nil);
    }

    uint32_t linkLength = 0;
    // Parse length field
    for (uint8_t lengthCounter = 0; lengthCounter < DOS_SYMLINK_LENGTH_LENGTH ; ++lengthCounter) {
        char c = symlinkBytes->length[lengthCounter];
        // Check if length is decimal
        if (c < '0' || c > '9')
        {
            // Length is non-decimal
            return reply(fs_errorForPOSIXError(EINVAL), nil);
        }
        linkLength = (10 * linkLength) + c - '0';
    }

    // Verify length is a valid
    if (linkLength > SYMLINK_LINK_MAX)
    {
        return reply(fs_errorForPOSIXError(EINVAL), nil);
    }

    NSData *digest = [Utilities getMD5Digest:32
                                     forData:symlinkBytes->link
                                      length:linkLength];

    // Verify the MD5 digest
    if (strncmp(digest.bytes, symlinkBytes->md5, 32) != 0) {
        return reply(fs_errorForPOSIXError(EINVAL), nil);
    }

    // It passed all the checks - must be a symlink
    // Make sure the link is null terminated. This might require us to add
    // one more byte to the buffer in case linkLength == SYMLINK_LINK_MAX
    if (linkLength == SYMLINK_LINK_MAX) {
        [linkData increaseLengthBy:1];
        symlinkBytes = (struct symlink *)linkData.mutableBytes;
    }

    symlinkBytes->link[linkLength] = '\0';
    NSString *symlinkStr = [NSString stringWithUTF8String:symlinkBytes->link];

    return reply(nil, symlinkStr);
}

-(void)purgeMetaBlocksFromCache:(void(^)(NSError * _Nullable))reply
{
    uint32_t sizeLeftToPurge = (uint32_t)ROUND_UP([self.entryData getValidDataLength], self.volume.systemInfo.bytesPerSector);
    NSMutableArray<FSMetadataBlockRange *> *rangesToPurge = [NSMutableArray array];
    uint32_t startCluster = self.firstCluster;
    __block uint32_t numOfContigClusters = 0;
    FSMetadataBlockRange *blockRange = nil;
    __block uint32_t nextCluster = 0;
    uint32_t totalNumOfClusters = 0;
    uint32_t availableLength = 0;
    NSError *error = nil;

    /*
     * The inner loop goes through the allocated clusters,
     * and saves up-to MAX_META_BLOCK_RANGES ranges in the array.
     * The outer loop calls synchronousMetadataPurge: for these ranges.
     * We exit both loops once we reach the end of the cluster chain.
     */
    while (sizeLeftToPurge && [self.volume.systemInfo isClusterValid:startCluster]) {
        while ([self.volume.systemInfo isClusterValid:startCluster] && [rangesToPurge count] < MAX_META_BLOCK_RANGES) {
            [self.volume.fatManager getContigClusterChainLengthStartingAt:startCluster
                                                             replyHandler:^(NSError * _Nullable error,
                                                                            uint32_t length,
                                                                            uint32_t nextClusterChainStart) {
                if (error) {
                    os_log_error(fskit_std_log(), "%s: Failed to get clusters chain. Error: %@", __func__, error);
                    return reply(error);
                }

                numOfContigClusters = length;
                nextCluster = nextClusterChainStart;
            }];

            totalNumOfClusters += numOfContigClusters;
            if (totalNumOfClusters > self.numberOfClusters) {
                os_log_error(fskit_std_log(), "%s: There are more clusters than expected, exiting", __func__);
                return reply(nil);
            }

            /*
             * Symlinks have one block per contiguous cluster chain. (which
             * doesn't necessarily cover the entire cluster chain).
             * Also, we don't know the block length in advance, it depends if
             * the symlink is contiguous or not.
             */
            availableLength = self.volume.systemInfo.bytesPerCluster * numOfContigClusters;
            blockRange = [FSMetadataBlockRange rangeWithOffset:[self.volume.systemInfo offsetForCluster:startCluster]
                                                   blockLength:MIN(availableLength, sizeLeftToPurge)
                                                numberOfBlocks:1];

            sizeLeftToPurge -= blockRange.blockLength;
            [rangesToPurge addObject:blockRange];

            if (sizeLeftToPurge == 0) {
                // We should purge what we got so far and exit,
                // as we've covered all of this symlink's metadata blocks.
                break;
            }

            /*
             * If the node is contiguous, nextCluster will be EOF, so it won't
             * be valid and we break from the loop.
             */
            startCluster = nextCluster;
        }

        error = [Utilities syncMetaPurgeToDevice:self.volume.resource
                           rangesToPurge:rangesToPurge];
        if (error) {
            os_log_error(fskit_std_log(), "%s: Couldn't purge meta blocks. Error = %@.", __FUNCTION__, error);
            break;
        }

        [rangesToPurge removeAllObjects];
    }
    return reply(error);
}

/*
 * SymLinkItem overrides getAttributes to override the size attribute.
 * All other attributes should be set by FATItem implementation
 */
-(FSItemAttributes *)getAttributes:(nonnull FSItemGetAttributesRequest *)desired
{
    FSItemAttributes *attrs = [super getAttributes:desired];

    if ([desired isWanted:FSItemAttributeType]) {
        attrs.type = FSItemTypeSymlink;
    }

    if ([desired isWanted:FSItemAttributeSize]) {
        attrs.size = self.symlinkLength;
    }
    
    return attrs;
}

@end
