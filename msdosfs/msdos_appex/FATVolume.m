/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#include <stdatomic.h>
#import <sys/attr.h>

#import "ExtensionCommon.h"
#import "DirNameCache.h"
#import "FATManager.h"
#import "ItemCache.h"
#import "FATVolume.h"
#import "FATItem.h"
#import "DirItem.h"

@implementation FATVolume

bool calculatedOffset = false;
int gmtOffset = 0;

+(int)GetGMTDiffOffset
{
    if (calculatedOffset) {
        return gmtOffset;
    }

    time_t rawTime = time(NULL);
    time_t gmt = time(NULL);
    struct tm *ptm = NULL;

    ptm = gmtime(&rawTime);
    /* Request that mktime() looks up dst in timezone database */
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    /* No need to repeat the calculation */
    calculatedOffset = true;

    return (int)difftime(rawTime, gmt);
}

-(instancetype)initWithResource:(FSResource *)resource
                       volumeID:(nonnull FSVolumeIdentifier *)volumeID
                     volumeName:(nonnull NSString *)volumeName
{
    self = [super initWithVolumeID:volumeID
                        volumeName:[FSFileName nameWithString:volumeName]];

    if (!self) {
        goto exit;
    }

    _resource = nil;
    if ([resource isKindOfClass:[FSBlockDeviceResource class]]) {
        _resource                       = (FSBlockDeviceResource *)resource;
    }
    _nameCachePool                  = [DirNameCachePool new];
    _nextAvailableFileID            = [[NSMutableArray alloc] initWithObjects:[NSNumber numberWithUnsignedLongLong:INITIAL_ZERO_LENGTH_FILEID], nil];
    _preAllocatedOpenFiles          = [[NSMutableArray alloc] initWithObjects:[NSNumber numberWithUnsignedLongLong:0], nil];
    _openUnlinkedFiles              = [[NSMutableArray alloc] initWithObjects:[NSNumber numberWithUnsignedLongLong:0], nil];

exit:
    return self;
}

#pragma mark internal methods

/*
 * GetFileID from DirEntryData should be used ONLY when we want to get the ID without building a FATItem.
 * It should not be used for anything related to the root directory.
 * For the root directory we must have a DitItem to get the ID.
 */
-(uint64_t)getFileID:(DirEntryData *)entryData
{
    uint64_t fileID = [entryData getFirstCluster:self.systemInfo];
    
    if (fileID == 0) {
        fileID = [self getNextAvailableFileID];
    }
    
    return fileID;
}

-(uint64_t)getNextAvailableFileID
{
    uint64_t fileid = 0;
    // Counter for file ids of zero-length files (starts at 0xFFFFFFFFFFFFFFFF and is decremented until wrapping around at 0xFFFFFFFF00000000)
    @synchronized (_nextAvailableFileID) {
        fileid = [[_nextAvailableFileID objectAtIndex:0] unsignedLongLongValue];
        if (fileid == WRAPAROUND_ZERO_LENGTH_FILEID) {
            [_nextAvailableFileID replaceObjectAtIndex:0 withObject:[NSNumber numberWithUnsignedLongLong:INITIAL_ZERO_LENGTH_FILEID]];
        } else {
            [_nextAvailableFileID replaceObjectAtIndex:0 withObject:[NSNumber numberWithUnsignedLongLong:(fileid - 1)]];
        }
    }
    
    return fileid;
}

- (NSError *)writeSymlinkClusters:(uint32_t)firstCluster
                      withContent:(nonnull NSData *)contents
{
    __block NSError * err = nil;
    uint64_t roundedSymlinkLength = ROUND_UP(sizeof(struct symlink), self.systemInfo.bytesPerSector);
    NSMutableData *linkContent = [[NSMutableData alloc] initWithLength:roundedSymlinkLength];

    if (linkContent == nil) {
        return fs_errorForPOSIXError(ENOMEM);
    }

    err = [SymLinkItem createSymlinkFromContent:contents
                                       inBuffer:linkContent];

    if (err) {
        return err;
    }

    uint64_t accWriteLength = 0;
    int64_t sizeLeftToWrite = roundedSymlinkLength;
    __block uint32_t startCluster = firstCluster;

    while ((sizeLeftToWrite > 0) && ![self.fatManager isEOFCluster:startCluster]) {

        uint64_t offset = [self.systemInfo offsetForCluster:startCluster];
        __block uint64_t availableLength;

        [self.fatManager getContigClusterChainLengthStartingAt:startCluster
                                                  replyHandler:^(NSError * _Nullable error, uint32_t numOfContigClusters, uint32_t nextCluster) {
            err = error;
            availableLength = self.systemInfo.bytesPerCluster * numOfContigClusters;
            startCluster = nextCluster;
        }];

        if (err) {
            os_log_error(fskit_std_log(), "%s: getContigClusterChainLengthStartingAt Failed", __FUNCTION__);
            break;
        }

        uint64_t writeLength = MIN(availableLength, sizeLeftToWrite);

        err = [Utilities metaWriteToDevice:self.resource
                                      from:(void*)((char*)linkContent.mutableBytes + accWriteLength)
                                startingAt:offset
                                    length:writeLength];
        if (err) {
            os_log_error(fskit_std_log(), "%s: Failed to write link content into the device", __FUNCTION__);
            break;
        }

        accWriteLength += writeLength;
        sizeLeftToWrite -= writeLength;
    }

    return err;
}

-(void)isSymLink:(DirEntryData *)dirEntryData
    replyHandler:(nonnull void (^)(bool isSymLink, NSString * _Nullable linkStr))reply
{
    if ([dirEntryData getValidDataLength] != sizeof(struct symlink)) {
        return reply(false, nil);
    }

    // If startCluster == 0 it must be a file.
    uint32_t startCluster = [dirEntryData getFirstCluster:self.systemInfo];
    if (startCluster == 0) {
        return reply(false, nil);
    }

    // Need to read the clusters to check if its a link or not
    uint64_t expectedNumOfClusters = roundup(sizeof(struct symlink), self.systemInfo.bytesPerCluster) / self.systemInfo.bytesPerCluster;
    uint64_t roundedSymlinkLength = roundup(sizeof(struct symlink), self.systemInfo.bytesPerSector);
    uint64_t accReadLength = 0;

    NSMutableData *linkContent = [[NSMutableData alloc] initWithLength:roundedSymlinkLength];
    int sizeLeftToRead = (int)roundedSymlinkLength;
    __block uint32_t totalNumOfClusters = 0;
    __block NSError *err = nil;

    while (![self.fatManager isEOFCluster:startCluster]) {
        __block uint64_t availableLength;
        __block uint32_t nextCluster;
        
        [self.fatManager getContigClusterChainLengthStartingAt:startCluster
                                                  replyHandler:^(NSError * _Nullable error, uint32_t numOfContigClusters, uint32_t next) {
            totalNumOfClusters += numOfContigClusters;
            availableLength = self.systemInfo.bytesPerCluster * numOfContigClusters;
            nextCluster = next;
            err = error;
        }];

        // Since link size is const, every file, that its cluster length not equals to link cluster length,
        // is automaticly not a link
        if (err || (expectedNumOfClusters > totalNumOfClusters)) {
            return reply(false, nil);
        }

        if (sizeLeftToRead > 0) {
            uint64_t readLength = MIN(availableLength, sizeLeftToRead);
            err = [Utilities syncMetaReadFromDevice:self.resource
                                               into:(void*)((char*)linkContent.mutableBytes + accReadLength)
                                         startingAt:[self.systemInfo offsetForCluster:startCluster]
                                             length:readLength];
            accReadLength += readLength;
            sizeLeftToRead -= readLength;
        }

        if (err) {
            os_log_error(fskit_std_log(), "%s: Failed to read link content", __FUNCTION__);
            return reply(false, nil);
        }

        startCluster = nextCluster;
    }

    if (expectedNumOfClusters != totalNumOfClusters)
    {
        return reply(false, nil);
    }

    __block NSString * linkString;
    [SymLinkItem verifyAndGetLink:linkContent
                     replyHandler:^(NSError * _Nullable error, NSString * _Nullable linkStr) {
        err = error;
        linkString = linkStr;
    }];

    return reply(err ? false : true, linkString);
}

-(FATItem *)createFATItemWithParent:(DirItem *)parentDirItem
                               name:(NSString *)name
                       dirEntryData:(DirEntryData *)dirEntryData
{
    __block bool isSymlink = false;
    __block uint16_t symlinkLength = 0;

    if (dirEntryData.type == FSItemTypeDirectory) {
        return [self createDirItemWithParent:parentDirItem
                                firstCluster:[dirEntryData getFirstCluster:self.systemInfo]
                                dirEntryData:dirEntryData
                                        name:name
                                      isRoot:false];
    } else if (dirEntryData.type == FSItemTypeFile) {
        [self isSymLink:dirEntryData
           replyHandler:^(bool isSymbolicLink, NSString * _Nullable linkStr) {
            isSymlink = isSymbolicLink;
            if (linkStr) {
                symlinkLength = strlen(linkStr.UTF8String);
            }
        }];

        if (isSymlink) {
            return [self createSymlinkItemWithParent:parentDirItem
                                        firstCluster:[dirEntryData getFirstCluster:self.systemInfo]
                                        dirEntryData:dirEntryData
                                                name:name
                                       symlinkLength:symlinkLength];
        }
        return [self createFileItemWithParent:parentDirItem
                                 firstCluster:[dirEntryData getFirstCluster:self.systemInfo]
                                 dirEntryData:dirEntryData
                                         name:name];
    } else {
        os_log_error(fskit_std_log(), "%s: got itemTypeUnknown.", __func__);
        return nil;
    }
}

#pragma mark internal methods

/*
 * This function clears (= fills with zeros) newly allocated directory clusters,
 * starting from firstClusterToClear. The amount of clusters to clear is determined
 * by the length of the cluster chain (under the assumption that new clusters are always
 * allocated from the end of the directory).
 * It does so by calling metaClear for the relevant range.
 */
-(NSError *)clearNewDirClustersFrom:(uint32_t)firstClusterToClear
							 amount:(uint32_t)numClustersToClear
{
	__block NSError *error = 0;
	__block uint32_t nextCluster = 0;
	__block uint32_t numContigClusters = 0;
	uint32_t startCluster = firstClusterToClear;
	uint32_t totalNumberOfClusters = 0;
	size_t dirBlockSize = self.systemInfo.dirBlockSize;
	size_t clusterSize = self.systemInfo.bytesPerCluster;
	size_t dirBlocksPerCluster = clusterSize / dirBlockSize;
	NSMutableArray<FSMetadataBlockRange *> *rangesToClear = [NSMutableArray array];

	/*
	 * The inner loop goes through the allocated clusters,
	 * and saves up-to MAX_META_BLOCK_RANGES ranges in the array.
	 * The outer loop calls metaClear for these ranges.
	 * We exit both loops once we reach the end of the cluster chain.
	 */
	while ([self.systemInfo isClusterValid:startCluster]) {
		while ([self.systemInfo isClusterValid:startCluster] && ([rangesToClear count] < MAX_META_BLOCK_RANGES)) {
            [self.fatManager getContigClusterChainLengthStartingAt:startCluster
                                                      replyHandler:^(NSError * _Nullable fatError,
                                                                     uint32_t length,
                                                                     uint32_t nextClusterInChain) {
				if (fatError) {
					error = fatError;
				} else {
					numContigClusters = length;
					nextCluster = nextClusterInChain;
				}
			}];
			if (error) {
				os_log_error(fskit_std_log(), "%s: Failed to get the next cluster(s). Error = %@.", __func__, error);
				return error;
			}
			totalNumberOfClusters += numContigClusters;
			if (totalNumberOfClusters > numClustersToClear) {
				os_log_error(fskit_std_log(), "%s: There are more clusters in this cluster chain than expected. Exiting.", __func__);
				return fs_errorForPOSIXError(EFAULT);
			}

            [rangesToClear addObject:[FSMetadataBlockRange rangeWithOffset:[self.systemInfo offsetForCluster:startCluster]
                                                               blockLength:(uint32_t)dirBlockSize
                                                            numberOfBlocks:(uint32_t)(dirBlocksPerCluster * numContigClusters)]];
			startCluster = nextCluster;
		}

		error = [Utilities syncMetaClearToDevice:self.resource
								   rangesToClear:rangesToClear];
		if (error) {
			os_log_error(fskit_std_log(), "%s: Failed to clear clusters. Error = %@.", __func__, error);
			return error;
		}

        [rangesToClear removeAllObjects];
	}
	return error;
}

/*
 * This function return FSItemGetAttributesRequest for fetching attributes needed
 * for creating a new directory entry.
 */
- (FSItemGetAttributesRequest *)getAttrRequestForNewDirEntry
{
    FSItemGetAttributesRequest *getAttr = [[FSItemGetAttributesRequest alloc] init];
    getAttr.wantedAttributes = FSItemAttributeMode | FSItemAttributeSize |
                               FSItemAttributeBirthTime | FSItemAttributeModifyTime |
                               FSItemAttributeAccessTime;
    return getAttr;
}

#pragma mark FSVolumePathConfOperations methods

/*
 * The following are arbitrary values and should be properly implemented by the
 * implementing file system.
 */
- (BOOL)isChownRestricted {
    return 0; //TODO: Verify this
}

- (int32_t)maxFileSizeInBits {
    return 33;
}

- (int32_t)maxLinkCount {
    return 1;
}

- (int32_t)maxNameLength {
    return 255;
}

- (BOOL)isLongNameTruncated {
    return false;
}

- (int32_t)maxXattrSizeInBits {
    return 0;
}



#pragma mark FSVolumeOperations methods

/*
 * The following are mandatory implementations, must be overridden by the
 * subclasses.
 */
- (void)createItemNamed:(FSFileName *)name
                   type:(FSItemType)type
            inDirectory:(nonnull FSItem *)directory
             attributes:(nonnull FSItemSetAttributesRequest *)newAttributes
           replyHandler:(nonnull void (^)(FSItem * _Nullable, FSFileName * _Nullable, NSError * _Nullable))reply
{
    /* Verify item type */
    if (type != FSItemTypeDirectory && type != FSItemTypeFile) {
        os_log_error(fskit_std_log(), "%s: got an invalid type (%ld).", __func__, type);
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

    [self createItemNamed:name
                     type:type
              inDirectory:directory
               attributes:newAttributes
                  content:nil
             replyHandler:^(FSItem * _Nullable newItem, FSFileName * _Nullable newItemName, NSError * _Nullable error) {
        return reply(newItem, newItemName, error);
    }];
}

- (void)createItemNamed:(FSFileName *)name
                   type:(FSItemType)type
            inDirectory:(nonnull FSItem *)directory
             attributes:(nonnull FSItemSetAttributesRequest *)newAttributes
                content:(FSFileName *)contents
           replyHandler:(nonnull void (^)(FSItem * _Nullable, FSFileName * _Nullable, NSError * _Nullable))reply
{
    __block NSError *error = nil;
    __block FSItem *newItem = nil;
    __block uint32_t firstCluster = 0;
    __block uint32_t numOfClusters = 0;
    __block uint64_t newEntryOffsetInDir = 0;
    uint32_t numClustersToAlloc = 0;
    uint64_t clusterSize = self.systemInfo.bytesPerCluster;
    bool zeroFill;

    if (name == nil || name.data.length == 0) {
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

    if ([Utilities isDotOrDotDot:(char *)name.data.bytes length:name.data.length]) {
        /* Creation of '.' or '..' is not allowed. */
        return reply(nil, nil, fs_errorForPOSIXError(EEXIST));
    }

    DirItem *parentDirItem = [DirItem dynamicCast:directory];
    if (!parentDirItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid directory", __func__);
        return reply(nil, nil, fs_errorForPOSIXError(ENOTDIR));
    }

    if (parentDirItem.isDeleted) {
        return reply(nil, nil, fs_errorForPOSIXError(ENOTDIR));
    }

    /* Verify attributes contain mode for FSItemTypeFile and FSItemTypeSymlink*/
    if (((type == FSItemTypeFile) || (type == FSItemTypeSymlink)) && (![newAttributes isValid:FSItemAttributeMode])) {
        os_log_error(fskit_std_log(), "%s: attributes don't contain a valid mode.", __func__);
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

    /* Verify file size */
    uint64_t fileSize = [newAttributes isValid:FSItemAttributeSize] ? newAttributes.size : 0;
    if (type == FSItemTypeFile) {
        error = [self verifyFileSize:fileSize];
        if (error) {
            os_log_error(fskit_std_log(), "%s: file size is invalid. Error = %@.", __func__, error);
            return reply(nil, nil, error);
        }
    }

    /* Set the dirty bit */
    [self.fatManager setDirtyBitValue:dirtyBitDirty
                         replyHandler:^(NSError * _Nullable fatError) {
        if (fatError) {
            /* Don't fail the creation. Just log the error. */
            os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __func__, fatError);
        }
    }];

    // Try to get a name cache for the parent directory. If there is no cache object available
    // at the moment, just continue without a cache and let future create/lookup try again.
    __block DirNameCache *nameCache = nil;
    __block bool nameCaheIsNew;

    [self.nameCachePool getNameCacheForDir:parentDirItem
                                cachedOnly:false
                              replyHandler:^(NSError * _Nullable poolError, DirNameCache * _Nullable cache, bool isNew) {
        if (poolError) {
            os_log_error(fskit_std_log(), "%s: Couldn't get dir name cache. Error = %@.", __func__, poolError);
        } else if (cache) {
            nameCache = cache;
            nameCaheIsNew = isNew;
        }
    }];

    // Fill the cache only if it is new (empty)
    if (nameCache && nameCaheIsNew) {
        error = [parentDirItem fillNameCache:nameCache];
        if (error) {
            os_log_error(fskit_std_log(), "%s: Couldn't fill dir name cache. Error = %@.", __func__, error);
            error = nil;
        }
    }

    __block bool itemExists = false;
    [parentDirItem lookupDirEntryNamed:name.string
                          dirNameCache:nameCache
                          lookupOffset:nil
                          replyHandler:^(NSError * _Nonnull lookupError,
                                         DirEntryData * _Nonnull dirEntryData) {
        if (!lookupError) {
            itemExists = true;
        } else if (lookupError.code != ENOENT) {
            error = lookupError;
        }
    }];
    if (itemExists) {
        os_log_debug(fskit_std_log(), "%s: item named %@ already exists.", __func__, name);
        error = fs_errorForPOSIXError(EEXIST);
        goto exit;
    }
    if (error) {
        os_log_error(fskit_std_log(), "%s: lookup in dir failed with error = %@.", __func__, error);
        goto exit;
    }

    // Allocate clusters for the new file.
    zeroFill = true; // TODO: for msdos do we still want to zero-fill?
    if (type == FSItemTypeDirectory) {
        numClustersToAlloc = 1;
    } else if (type == FSItemTypeFile) {
        numClustersToAlloc = (uint32_t)(roundup(fileSize, clusterSize) / clusterSize);
    } else if (type == FSItemTypeSymlink) {
        numClustersToAlloc = (uint32_t)(roundup(sizeof(struct symlink), clusterSize) / clusterSize);
        // Allocate clusters for the new symlink, without zero-filling them.
        // (as we are about to write valid data up-to validDataLength).
        zeroFill = false;
    }

    if (numClustersToAlloc) {
        [self.fatManager allocateClusters:numClustersToAlloc
                             allowPartial:false
                                 zeroFill:zeroFill
                             mustBeContig:false
                             replyHandler:^(NSError * _Nullable allocationError,
                                            uint32_t firstAllocatedCluster,
                                            uint32_t lastAllocatedCluster,
                                            uint32_t numAllocated) {
            if (allocationError) {
                error = allocationError;
            } else if (numAllocated != numClustersToAlloc) {
                /* Shouldn't happen, just a sanity check. */
                os_log_fault(fskit_std_log(), "%s: %u clusters were allocated, while asked for %u. (allowPartial = false).",
                             __func__, numAllocated, numClustersToAlloc);
                error = fs_errorForPOSIXError(EFAULT);
            } else {
                firstCluster = firstAllocatedCluster;
                numOfClusters = numAllocated;
            }
        }];
        if (error) {
            os_log_error(fskit_std_log(), "%s: allocate clusters failed with error = %@.", __func__, error);
            goto exit;
        }
    }

	if (type == FSItemTypeDirectory) {
		/* Clear the newly allocated cluster */
		error = [self clearNewDirClustersFrom:firstCluster
									   amount:numOfClusters];
		if (error) {
			os_log_error(fskit_std_log(), "%s: clear dir clusters failed with error = %@.", __func__, error);
		}
	}

    if (type == FSItemTypeSymlink) {
        error = [self writeSymlinkClusters:firstCluster
                               withContent:contents.data];
        if (error) {
            os_log_error(fskit_std_log(), "%s: writeSymlinkClusters ended with error  %@.", __func__, error);
        }
    }

    if (error == nil) {
        /* Create dir entry in the parent directory. */
        [parentDirItem createNewDirEntryNamed:name.string
                                         type:type
                                   attributes:newAttributes
                             firstDataCluster:firstCluster
                                 replyHandler:^(NSError * _Nullable newEntryError,
                                                uint64_t offsetInDir) {
            if (newEntryError) {
                error = newEntryError;
            } else {
                newEntryOffsetInDir = offsetInDir;
            }
        }];
        if (error) {
            os_log_error(fskit_std_log(), "%s: create new entry failed with error = %@.", __func__, error);
        }
    }

    if (error == nil) {
        /* Lookup for the new file. */
        [parentDirItem lookupDirEntryNamed:name.string
                              dirNameCache:nil
                              lookupOffset:&newEntryOffsetInDir
                              replyHandler:^(NSError *lookupError,
                                             DirEntryData *dirEntryData) {
            if (lookupError) {
                error = lookupError;
            } else {
                newItem = [self createFATItemWithParent:parentDirItem
                                                   name:name.string
                                           dirEntryData:dirEntryData];
            }
        }];
        if (error) {
            os_log_error(fskit_std_log(), "%s: lookup for new item failed with error = %@.", __func__, error);
        }
    }

    if (error == nil) {
        if (type == FSItemTypeDirectory) {
            /* Create '.' and '..' entries in the new directory if needed */
            DirItem *newDirItem = [DirItem dynamicCast:newItem];
            error = [newDirItem createDotEntriesWithAttrs:newAttributes];
            if (error) {
                os_log_error(fskit_std_log(), "%s: failed to create '.' and '..' entries in the new dir. Error = %@.", __func__, error);
            }
        }
    }

    if (error != nil) {
        /* Free the newly allocated clusters in case of an error. */
        [self.fatManager freeClusterFrom:firstCluster
                             numClusters:numOfClusters
                            replyHandler:^(NSError * _Nullable freeClustersError) {
            if (freeClustersError) {
                /* We don't have what to do with this error except for log it. */
                os_log_error(fskit_std_log(), "%s: free clusters of the new item failed with error = %@.", __func__, freeClustersError);
            }
        }];
        goto exit;
    }

    /*
     * In this point we're commited to the creation.
     * We must return success if we got here.
     */

    /* Update parent dir modification time if needed. */
    error = [parentDirItem updateModificationTimeOnCreateRemove];
    if (error) {
        /* In case the update failed, we still want the create to succeed. */
        os_log_error(fskit_std_log(), "%s: update parent dir modification time failed with error = %@.", __func__, error);
    }

    if (nameCache) {
        /* insert new item to name cache. */
        error = [nameCache insertDirEntryNamed:(char *)name.data.bytes
                                      ofLength:name.data.length
                                   offsetInDir:newEntryOffsetInDir];
        if (error) {
            /* In case the insert failed, we still want the create to succeed.
             so we just log, and mark the name cache as incomplete. */
            os_log_error(fskit_std_log(), "%s: insert the new item to name cache failed with error = %@.", __func__, error);
            nameCache.isIncomplete = true;
        }
    }

    /* Insert the new item to the items cache */
    if (newItem) {
        [_itemCache insertItem:(FATItem *)newItem
                  replyHandler:^(FATItem * _Nullable cachedItem,
                                 NSError * _Nullable innerError) {
            if (innerError) {
                error = innerError;
            } else {
                /*
                 * In the case of remove and new creation, because the item
                 * wasn't necessarily reclaimed, we will have an existing
                 * item in the cache.
                 */
                newItem = cachedItem;
            }
        }];
    }

exit:
    if (nameCache) {
        [self.nameCachePool doneWithNameCacheForDir:parentDirItem];
    }

	return reply(newItem, nil, error);
}

-(void)createFileNamed:(FSFileName *)name
           inDirectory:(FSItem *)directory
            attributes:(FSItemSetAttributesRequest *)newAttributes
           usingPacker:(FSExtentPacker)packer
          replyHandler:(void (^)(FSItem * _Nullable, FSFileName * _Nullable, NSError * _Nullable))reply
{
    // Return zero extents for now.
    [self createItemNamed:name
                     type:FSItemTypeFile
              inDirectory:directory
               attributes:newAttributes
             replyHandler:^(FSItem * _Nullable newItem, FSFileName * _Nullable newItemName, NSError * _Nullable error) {
        return reply(newItem, newItemName, error);
    }];
}

- (void)createLinkToItem:(nonnull FSItem *)item
                   named:(FSFileName *)name
             inDirectory:(nonnull FSItem *)directory
            replyHandler:(nonnull void (^)(FSFileName * _Nullable linkName,
                                           NSError * _Nullable))reply
{
    reply(nil, fs_errorForPOSIXError(ENOTSUP));
}

- (void)createSymbolicLinkNamed:(FSFileName *)name
                    inDirectory:(nonnull FSItem *)directory
                     attributes:(nonnull FSItemSetAttributesRequest *)newAttributes
                   linkContents:(nonnull FSFileName *)contents
                   replyHandler:(nonnull void (^)(FSItem * _Nullable, FSFileName * _Nullable, NSError * _Nullable))reply
{
    if ((contents == nil) || (contents.data.length == 0) || (contents.data.length > SYMLINK_LINK_MAX)) {
        os_log_error(fskit_std_log(), "%s: got an invalid contents", __func__);
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

    [self createItemNamed:name
                     type:FSItemTypeSymlink
              inDirectory:directory
               attributes:newAttributes
                  content:contents
             replyHandler:^(FSItem * _Nullable newItem,
                            FSFileName * _Nullable newItemName,
                            NSError * _Nullable error) {
        return reply(newItem, newItemName, error);
    }];
}

- (void)adjustCookieIndex:(uint32_t *)cookieIndex
                  dirItem:(DirItem *)dirItem
        provideAttributes:(bool)provideAttributes
{
    if (dirItem.isRoot && !provideAttributes && *cookieIndex >= NUM_DOT_ENTRIES) {
        /*
         * Adapt the start index:
         * If index is greater than DOT or DOTDOT index, we already synthesized
         * those entries, so we need to count less than we were asked.
         */
        *cookieIndex -= NUM_DOT_ENTRIES;
    }
    if (!dirItem.isRoot && provideAttributes) {
        /*
         * For non-root directory, when reading attributes, we need to skip the
         * DOT and DOTDOT entries, because they aren't reported back in this case.
         */
        *cookieIndex += NUM_DOT_ENTRIES;
    }
}

- (void)enumerateDirectory:(nonnull FSItem *)directory
          startingAtCookie:(FSDirectoryCookie)cookie
                  verifier:(FSDirectoryVerifier)verifier
       providingAttributes:(FSItemGetAttributesRequest * _Nullable)attributes
                usingBlock:(nonnull FSDirectoryEntryPacker)packer
              replyHandler:(nonnull void (^)(FSDirectoryVerifier, NSError * _Nullable))reply
{
    DirItem *dirItem = [DirItem dynamicCast:directory];
    if (!dirItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item", __FUNCTION__);
        return reply(0, fs_errorForPOSIXError(ENOTDIR));
    }

    if (dirItem.isDeleted) {
        return reply(0, nil);
    }

    bool provideAttributes = (attributes != nil);
    __block bool synthesizeDot = false;
    __block bool synthesizeDotDot = false;
    __block NSError *err = nil;
    __block uint32_t cookieOffset = OFFSET_FROM_COOKIE(cookie);
    __block uint32_t cookieIndex = INDEX_FROM_COOKIE(cookie);
    __block uint32_t nextCookieOffset = 0;

    if (dirItem.isRoot && !provideAttributes) {
        if ((cookieOffset == DOT_COOKIE) || (cookieOffset == DOT_DOT_COOKIE )) {
            synthesizeDot = (cookieOffset == DOT_COOKIE);
            synthesizeDotDot = true;
            cookieOffset = 0;
        }
        else {
            cookieOffset -= SYNTHESIZE_ROOT_DOTS_SIZE;
        }
        nextCookieOffset = SYNTHESIZE_ROOT_DOTS_SIZE;
    }

    if ((cookieOffset % [dirItem dirEntrySize]) != 0) {
        // Something is really wrong if we get here so log fault
        os_log_fault(fskit_std_log(), "%s: cookieOffset [%u] not aligned with dirEntrySize [%u]",
                     __FUNCTION__, cookieOffset, [dirItem dirEntrySize]);
        err = fs_errorForPOSIXError(EINVAL);
    }

    /*
     * In case the dir version has been changed, we suspect that the cookie-offset
     * may be invalid. Therefore we perform some more checks in this case. We skip the
     * checks if we are still synthesizing dir entries.
     * In case the dir version hasn't been changed, we won't check the validity to not
     * return an error, because we want to do our best enumerating corrupted directories.
     * We would just try to skip the corrupted dir entries if we encounter any.
     * In case the dir version has been changed, we do check the validity, because
     * dir entries may have been added/removed during the enumeration, so in case
     * the offset is invalid we ignore the given offset and calculate a new one
     * using the cookie-index.
     */
    if ((dirItem.dirVersion != verifier) && (!synthesizeDot) && (!synthesizeDotDot)) {
        err = (err) ? err : [dirItem verifyCookieOffset:cookieOffset];
        if (err) {
            __block bool eof;
            // The cookie-offset doesn't point to a valid dir entry.
            // We continue anyway by fetching the offset of the desired entry,
            // using the cookie-index which holds the desired dir-entry index.
            os_log_error(fskit_std_log(), "%s: Failed to verifyCookieOffset, calling get dir entry by index.", __FUNCTION__);
            [self adjustCookieIndex:&cookieIndex dirItem:dirItem provideAttributes:provideAttributes];
            [dirItem getDirEntryOffsetByIndex:cookieIndex
                                 replyHandler:^(NSError * _Nullable error, uint64_t offset, bool reachedEOF) {
                err = error;
                cookieOffset = (uint32_t)offset;
                eof = reachedEOF;
                if (dirItem.isRoot && !provideAttributes && ((cookieOffset == DOT_COOKIE) || (cookieOffset == DOT_DOT_COOKIE ))) {
                    /* cookieOffset has been changed, need to re-think if we need to synthesize DOT and DOTDOT*/
                    synthesizeDot = (cookieOffset == DOT_COOKIE);
                    synthesizeDotDot = true;
                }
            }];

            if (err || eof) {
                return reply(0, err);
            }
        }
    } else if (err) {
        /* We got one bad cookie here */
        os_log_error(fskit_std_log(), "%s: Bad cookie", __FUNCTION__);
        err = [NSError errorWithDomain:FSVolumeErrorDomain
                                  code:FSVolumeErrorBadDirectoryCookie
                              userInfo:nil];
    }

    if (err) {
        return reply(0, err);
    }

    __block NSString *nameToPack = nil;
    __block FSItemAttributes *attrToPack = nil;
    __block FSItemType typeToPack;
    __block uint64_t idToPack;

    if (synthesizeDot) {
        // We know that every directory must have '.' and '..' so the '.' is NOT the last dir entry to pack
        cookieIndex++;
        int packRes = packer([FSFileName nameWithCString:"."], FSItemTypeDirectory,  [dirItem getFileID] , COOKIE_FROM_OFFSET_AND_INDEX(DOT_DOT_COOKIE, cookieIndex), nil, false);
        if (packRes) {
            return reply(dirItem.dirVersion, err);
        }
    }

    // From this point we will only know if the entry we are about to pack is the last entry, when we finish
    // iterating the directory. That is why we always keep the information about the what we want to pack
    // but we actually pack it after we know that this is not the last entry in the directory.
    if (synthesizeDotDot) {
        nameToPack = [[NSString alloc] initWithUTF8String:".."];
        typeToPack = FSItemTypeDirectory;
        idToPack = [dirItem isRoot] ? [self getNextAvailableFileID] : [dirItem.parentDir getFileID];
    }

    __block NSMutableData *utf8Data = [NSMutableData dataWithLength:FAT_MAX_FILENAME_UTF8];
    [dirItem iterateFromOffset:cookieOffset
                       options:0
                  replyHandler:^iterateDirStatus(NSError * _Nonnull error,
                                                 dirEntryType result,
                                                 uint64_t dirEntryOffset,
                                                 struct unistr255 * _Nullable utf16Name,
                                                 DirEntryData * _Nullable dirEntryData) {
        if (error) {
            err = error;
            os_log_error(fskit_std_log(), "%s iterateFromOffset error %d.\n", __FUNCTION__, (int)error.code);
            return iterateDirStop;
        }

        if (result == FATDirEntryFound) {
            int packerRes = 0;

            if (nameToPack) {
                uint64_t cookie = nextCookieOffset + dirEntryOffset;
                cookieIndex++;
                cookie = COOKIE_FROM_OFFSET_AND_INDEX(cookie, cookieIndex);
                packerRes = packer([FSFileName nameWithString:nameToPack], typeToPack, idToPack, cookie, attrToPack, false);
                nameToPack = nil;
            }

            if (packerRes) {
                return iterateDirStop;
            }
            CONV_Unistr255ToUTF8(utf16Name, utf8Data.mutableBytes);
            NSString *name = [NSString stringWithUTF8String:utf8Data.mutableBytes];
            if ((name == nil) ||
                (name.length == 0)) {
                /* We don't want to pack a nameless direntry */
                return iterateDirContinue;
            }
            if (provideAttributes) {
                if ([Utilities isDotOrDotDot:(char*)name.UTF8String length:name.length]) {
                    return iterateDirContinue;
                }

                FATItem *tmpItem = [self createFATItemWithParent:dirItem
                                                   name:name
                                           dirEntryData:dirEntryData];
                attrToPack = [tmpItem getAttributes:attributes];
                typeToPack = attrToPack.type;
            } else if (dirEntryData.type == FSItemTypeDirectory) {
                typeToPack = FSItemTypeDirectory;
            } else {
                [self isSymLink:dirEntryData
                   replyHandler:^(bool isSymbolicLink, NSString * _Nullable linkStr) {
                    typeToPack = isSymbolicLink ? FSItemTypeSymlink : FSItemTypeFile;
                }];
            }

            nameToPack = name;
            idToPack = [self getFileID:dirEntryData];
        } else if (result == FATDirEntryEmpty) {
            return iterateDirStop;
        }

        return iterateDirContinue;
    }];

    if (nameToPack) {
        // If we still have something to pack it is the last entry in the directory.
        packer([FSFileName nameWithString:nameToPack], typeToPack, idToPack, 0, attrToPack, true);
    }

    return reply(dirItem.dirVersion, err);
}

- (void)getAttributes:(FSItemGetAttributesRequest *)desiredAttributes
               ofItem:(FSItem *)item
         replyHandler:(void (^)(FSItemAttributes * _Nullable, NSError * _Nullable))reply
{
    FATItem *fatItem = [FATItem dynamicCast:item];
    
    if (!fatItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item.", __FUNCTION__);
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }
    
    FSItemAttributes *attrs = [fatItem getAttributes:desiredAttributes];

    return reply(attrs, nil);
    
}

- (void)lookupItemNamed:(FSFileName *)name
            inDirectory:(nonnull FSItem *)directory
           replyHandler:(nonnull void (^)(FSItem * _Nullable, FSFileName * _Nullable, NSError * _Nullable))reply
{
	__block NSError *error = nil;

    /* Sanity:  Some encoding can end up in a nil name */
    if (name == nil) {
        os_log_error(fskit_std_log(), "%s: name is nil", __FUNCTION__);
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

	DirItem *dirItem = [DirItem dynamicCast:directory];
	if (!dirItem) {
		os_log_error(fskit_std_log(), "%s: got an invalid directory item.", __FUNCTION__);
		return reply(nil, nil, fs_errorForPOSIXError(ENOTDIR));
	}

    if (name == nil || name.string == nil) {
        return reply(nil, nil, fs_errorForPOSIXError(EINVAL));
    }

    if (dirItem.isDeleted) {
        return reply(nil, nil, fs_errorForPOSIXError(ENOENT));
    }

    if ([Utilities isDotOrDotDot:(char *)name.data.bytes length:name.data.length]) {
        /* Lookup for '.' or '..' is not allowed. */
        return reply(nil, nil, fs_errorForPOSIXError(EPERM));
    }

    // Try to get a name cache for the parent directory. If there is no cache object available
    // at the moment, just continue without a cache and let future create/lookup try again.
    __block DirNameCache *nameCache = nil;
    __block bool nameCaheIsNew;

    [self.nameCachePool getNameCacheForDir:dirItem
                                cachedOnly:false
                              replyHandler:^(NSError * _Nullable poolError, DirNameCache * _Nullable cache, bool isNew) {
        if (poolError) {
            os_log_error(fskit_std_log(), "%s: Couldn't get dir name cache. Error = %@.", __func__, poolError);
        } else if (cache) {
            nameCache = cache;
            nameCaheIsNew = isNew;
        }
    }];

    // Fill the cache only if it is new (empty)
    if (nameCache && nameCaheIsNew) {
        error = [dirItem fillNameCache:nameCache];
        if (error) {
            os_log_error(fskit_std_log(), "%s: Couldn't fill dir name cache. Error = %@.", __func__, error);
            error = nil;
        }
    }

	__block FATItem *resItem = nil;
	[dirItem lookupDirEntryNamed:name.string
					dirNameCache:nameCache
					lookupOffset:nil
                    replyHandler:^(NSError * _Nonnull dirLookupError,
								   DirEntryData * _Nonnull dirEntryData) {
		if (dirLookupError) {
			error = dirLookupError;
		} else {
            resItem = [self createFATItemWithParent:dirItem
                                               name:name.string
                                       dirEntryData:dirEntryData];
		}
	}];

	if (nameCache) {
		[self.nameCachePool doneWithNameCacheForDir:dirItem];
	}

    if (resItem != nil) {
        /* Insert to the item cache */
        [_itemCache insertItem:resItem
                  replyHandler:^(FATItem * _Nullable cachedItem,
                                 NSError * _Nullable innerError) {
            if (innerError) {
                error = innerError;
            } else {
                resItem = cachedItem;
            }
        }];
    }

    if (resItem == nil && error == nil) {
        /* We don't want to reply a nil object without some error */
        error = fs_errorForPOSIXError(EINVAL);
    }

	return reply(resItem, nil, error);
}

-(void)lookupItemNamed:(FSFileName *)name
           inDirectory:(FSItem *)directory
           usingPacker:(FSExtentPacker)packer
          replyHandler:(void (^)(FSItem * _Nullable,
                                 FSFileName * _Nullable,
                                 NSError * _Nullable)) reply
{
    [self lookupItemNamed:name
              inDirectory:directory
             replyHandler:^(FSItem * _Nullable theItem, FSFileName * _Nullable itemName, NSError * _Nullable error) {
        if (!error) {
            FileItem *fileItem = [FileItem dynamicCast:theItem];
            if (fileItem) {
                [fileItem fetchFileExtentsFrom:0
                                            to:[fileItem.entryData getSize]
                               lastValidOffset:[fileItem.entryData getValidDataLength]
                                   usingBlocks:packer
                                  replyHandler:^(NSError * _Nullable fetchError) {
                    if (fetchError) {
                        // Just log the error. Don't fail the lookup operation.
                        os_log_error(fskit_std_log(), "%s: Failed to fetch extents for file named %@. Error = %@.", __FUNCTION__, name, fetchError);
                    }
                }];
            }
        }
        return reply(theItem, itemName, error);
    }];
}

-(FSStatFSResult *)volumeStatistics
{
    FSStatFSResult *result = [[FSStatFSResult alloc] initWithFSTypeName:self.systemInfo.fsTypeName];
    result.blockSize = self.systemInfo.bytesPerCluster;
    result.ioSize = 32 * 1024; // Size (in bytes) of the optimal transfer block size
    result.totalBlocks = self.systemInfo.maxValidCluster;
    result.availableBlocks = self.systemInfo.freeClusters;
    result.freeBlocks = self.systemInfo.freeClusters;
    result.usedBlocks = self.systemInfo.maxValidCluster - self.systemInfo.freeClusters;
    result.totalFiles = 0;
    result.freeFiles = 0;
    result.filesystemSubType = self.systemInfo.fsSubTypeNum.intValue;

    return result;
}

-(FSVolumeSupportedCapabilities *)supportedVolumeCapabilities
{
    FSVolumeSupportedCapabilities *capabilities         = [FSVolumeSupportedCapabilities new];
    capabilities.supportsSymbolicLinks                  = YES;
    capabilities.supportsCasePreservingNames            = YES;
    capabilities.supportsHiddenFiles                    = YES;
    capabilities.doesNotSupportRootTimes                = YES;
    capabilities.doesNotSupportSettingFilePermissions   = YES;
    return capabilities;
}

- (void)readSymbolicLink:(nonnull FSItem *)item
            replyHandler:(nonnull void (^)(FSFileName * _Nullable,
                                           NSError * _Nullable))reply
{
    /* Verify item type */
    SymLinkItem *symlinkItem = [SymLinkItem dynamicCast:item];

    if (symlinkItem == nil) {
        os_log_error(fskit_std_log(), "%s: got an invalid type", __func__);
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    uint64_t roundedSymlinkLength = ROUND_UP(sizeof(struct symlink), self.systemInfo.bytesPerSector);
    NSMutableData *linkContent = [[NSMutableData alloc] initWithLength:roundedSymlinkLength];
    __block int sizeLeftToRead = (int)roundedSymlinkLength;
    __block uint64_t accReadLength = 0;
    __block uint64_t readLength = 0;
    __block NSError *error = nil;

    [self.fatManager iterateClusterChainOfItem:(FATItem*)item
                                  replyHandler:^iterateClustersResult(NSError * _Nullable err, uint32_t startCluster, uint32_t numOfContigClusters) {
        if (err) {
            error = err;
            return iterateClustersStop;
        }

        uint64_t availableLength = self.systemInfo.bytesPerCluster * numOfContigClusters;
        readLength = MIN(availableLength, sizeLeftToRead);
        error = [Utilities syncMetaReadFromDevice:self.resource
                                             into:(void*)((char*)linkContent.mutableBytes + accReadLength)
                                       startingAt:[self.systemInfo offsetForCluster:startCluster]
                                           length:readLength];
        if (error) {
            return iterateClustersStop;
        }

        accReadLength += readLength;
        sizeLeftToRead -= readLength;
        return (sizeLeftToRead > 0) ? iterateClustersContinue : iterateClustersStop;
    }];

    __block NSString *linkString = nil;
    [SymLinkItem verifyAndGetLink:linkContent
                     replyHandler:^(NSError * _Nullable err, NSString * _Nullable linkStr) {
        error = err;
        linkString = linkStr;
    }];

    return reply([FSFileName nameWithString:linkString], error);
}

- (void)reclaimItem:(nonnull FSItem *)item
       replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    FATItem *reclaimedItem = [FATItem dynamicCast:item];

    if (!reclaimedItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item.", __func__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    return reply([reclaimedItem reclaim]);
}

- (void)removeItem:(nonnull FSItem *)item
             named:(FSFileName *)name
     fromDirectory:(nonnull FSItem *)directory
      replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    FATItem *victimItem = [FATItem dynamicCast:item];
    DirItem *dirItem = [DirItem dynamicCast:directory];
    DirItem *dirVictimItem = nil;
    __block NSError *err = nil;

    if (!victimItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item.", __FUNCTION__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    if (!dirItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid directory item.", __FUNCTION__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    if (name == nil) {
        os_log_error(fskit_std_log(), "%s: got an invalid name.", __FUNCTION__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    if ([Utilities isDotOrDotDot:(char *)name.data.bytes length:name.data.length]) {
        /* Removing '.' or '..' is not allowed. */
        return reply(fs_errorForPOSIXError(EPERM));
    }

    if (victimItem.isDeleted) {
        return reply(nil);
    }

    if (victimItem.entryData.type == FSItemTypeDirectory) {
         dirVictimItem = [DirItem dynamicCast:victimItem];

        if (!dirVictimItem) {
            os_log_error(fskit_std_log(), "%s: got an invalid item.", __FUNCTION__);
            return reply(fs_errorForPOSIXError(EINVAL));
        }

        err = [dirVictimItem checkIfEmpty];
        if (err) {
            return reply(err);
        }
    }

    /*
     * Purge first, because if this fails, we can't write these clusters
     * to disk, as we free them later on.
     */
    if (dirVictimItem) {
        [dirVictimItem purgeMetaBlocksFromCache:^(NSError * _Nullable error) {
            if (error) {
                os_log_fault(fskit_std_log(), "%s: Failed to purge dir's metadata blocks, error %@", __func__, error);
                return reply(error);
            }
        }];
    }

    // Mark all node directory entries as deleted
    err = [dirItem markDirEntriesAsDeletedAndUpdateMtime:victimItem];

    if (err) {
        return reply(err);
    }

    __block DirNameCache *dirNameCache = nil;

    [self.nameCachePool getNameCacheForDir:dirItem
                                cachedOnly:true
                              replyHandler:^(NSError * _Nullable poolError, DirNameCache * _Nullable cache, bool isNew) {
        // We do not need to do anything if we did not get a name cache for any reason
        dirNameCache = cache;
    }];

    if (dirNameCache) {
        [dirNameCache removeDirEntryNamed:(char *)name.data.bytes
                                 ofLength:name.data.length
                              offsetInDir:victimItem.entryData.firstEntryOffsetInDir];
        [self.nameCachePool doneWithNameCacheForDir:dirItem];
    }

    [self.itemCache removeItem:victimItem];

    dirItem.dirVersion++;
    // TBD - Remove from HT only if it exist, do not create a new one just to remove an item

    if (dirVictimItem) {
        [dirItem.volume.fatManager freeClusters:dirVictimItem.numberOfClusters
                                         ofItem:dirVictimItem
                                   replyHandler:^(NSError * _Nullable error) {
            err = error;
        }];
    }

    [victimItem setDeleted];

    return reply(err);
}

- (void)renameItem:(nonnull FSItem *)sourceItem
       inDirectory:(nonnull FSItem *)sourceDirectory
             named:(FSFileName *)sourceName
         toNewName:(FSFileName *)destinationName
       inDirectory:(nonnull FSItem *)destinationDirectory
          overItem:(FSItem * _Nullable)overItem
      replyHandler:(nonnull void (^)(FSFileName * _Nullable, NSError * _Nullable))reply
{
    __block uint64_t newEntryOffset = 0;
    __block NSError *err = nil;
    __block FATItem *overFatItem = nil;
    __block DirEntryData *newDirEntryData = nil;
    __block DirEntryData *dotDotEntryData = nil;
    DirEntryData *sourceDirEntryData = nil;;
    DirEntryData *toDirEntryData = nil;;
    DirItem *dirFromItem = nil;;
    DirItem *overDirItem = nil;;
    bool isFromSymlink;

    if ((sourceItem == nil)                     ||
        (sourceDirectory == nil)                ||
        (destinationDirectory == nil)           ||
        (sourceName == nil)                     ||
        (destinationName == nil)                ||
        ([Utilities isDotOrDotDot:(char *)sourceName.data.bytes length:sourceName.data.length])  ||
        ([Utilities isDotOrDotDot:(char *)destinationName.data.bytes length:destinationName.data.length])) {
        os_log_error(fskit_std_log(), "%s: invalid argument", __FUNCTION__);
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    if ((sourceDirectory == destinationDirectory) &&
        ([sourceName.string isEqualToString:destinationName.string])) {
        os_log_debug(fskit_std_log(), "%s: source and destination are the same", __FUNCTION__);
        // Nothing to do if names are the same
        return reply(nil, nil);
    }

    FATItem *sourceFatItem = [FATItem dynamicCast:sourceItem];

    if (!sourceFatItem || sourceFatItem.isDeleted) {
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    DirItem *sourceDirItem = [DirItem dynamicCast:sourceDirectory];
    if (!sourceDirItem || sourceDirItem.isDeleted) {
        return reply(nil, fs_errorForPOSIXError(ENOTDIR));
    }
    
    DirItem *dstDirItem = [DirItem dynamicCast:destinationDirectory];
    if (!dstDirItem || dstDirItem.isDeleted) {
        return reply(nil, fs_errorForPOSIXError(ENOTDIR));
    }

    if (![[sourceFatItem.name lowercaseString] isEqualToString:[sourceName.string lowercaseString]]) {
        return reply(nil, fs_errorForPOSIXError(ENOENT));
    }

    sourceDirEntryData = sourceFatItem.entryData;
    if (sourceDirEntryData.type == FSItemTypeDirectory) {
        /*
         * Make sure we're not trying to move a directory into one of its
         * subdirectories. For this we'll iterate up the parents hierarchy
         * up to the root node to verify.
         */
        DirItem *tmp = [DirItem dynamicCast:dstDirItem.parentDir];
        while (tmp != nil) {
            if (tmp.firstCluster == sourceFatItem.firstCluster) {
                os_log_error(fskit_std_log(), "%s: Can't move a directory into its subdirectory", __FUNCTION__);
                return reply(nil, fs_errorForPOSIXError(EINVAL));
            }
            tmp = [DirItem dynamicCast:tmp.parentDir];
        }
    }

    /* Set the dirty bit */
    [self.fatManager setDirtyBitValue:dirtyBitDirty
                         replyHandler:^(NSError * _Nullable fatError) {
        if (fatError) {
            /* Don't fail the rename. Just log the error. */
            os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __func__, fatError);
        }
    }];

    // Get source attributes needed for creating a new directory entry
    FSItemGetAttributesRequest *getAttr = [self getAttrRequestForNewDirEntry];
    FSItemAttributes *sourceAttr = [sourceFatItem getAttributes:getAttr];

    __block DirNameCache *srcNameCache = nil;
    __block DirNameCache *dstNameCache = nil;

    [self.nameCachePool getNameCacheForDir:dstDirItem
                                cachedOnly:true
                              replyHandler:^(NSError * _Nullable poolError, DirNameCache * _Nullable cache, bool isNew) {
        // We do not need to do anything if we did not get a name cache for any reason
        dstNameCache = cache;
    }];

    /*
     * If the source and destination children are the same, then it is a rename
     * that may be changing the case of the name. For that, we pretend like
     * there was no destination child (so we don't try to remove it before
     * constructing the destination's directory entry set).
     */
    if (sourceItem != overItem) {
        if (overItem != nil) {
            overFatItem = [FATItem dynamicCast:overItem];
            if (!overFatItem || overFatItem.isDeleted) {
                err = fs_errorForPOSIXError(EINVAL);
                goto exit;
                
            }
            
        } else {
            // Get overItem if exist.
            [dstDirItem lookupDirEntryNamed:destinationName.string
                               dirNameCache:dstNameCache
                               lookupOffset:nil
                               replyHandler:^(NSError * _Nonnull dirLookupError,
                                              DirEntryData * _Nonnull dirEntryData) {
                if (dirLookupError) {
                    err = dirLookupError;
                } else {
                    overFatItem = [self createFATItemWithParent:dstDirItem
                                                           name:destinationName.string
                                                   dirEntryData:dirEntryData];
                }
            }];

            if (err) {
                if (err.code != ENOENT) {
                    os_log_error(fskit_std_log(), "%s: lookupName destinationName returned an error %@", __FUNCTION__, err);
                    goto exit;
                }
                err = nil;
            }
        }
    }

    if (overFatItem) {
        // In case we are going to override existing item, we need to remove it before performing the actual rename
        toDirEntryData = overFatItem.entryData;
        // ensure the objects are compatible
        if (sourceDirEntryData.type != toDirEntryData.type) {
            if (sourceDirEntryData.type == FSItemTypeDirectory) {
                os_log_error(fskit_std_log(), "%s: 'To' is not a directory", __FUNCTION__);
                err = fs_errorForPOSIXError(ENOTDIR);
                goto exit;
            } else if (toDirEntryData.type == FSItemTypeDirectory) {
                os_log_error(fskit_std_log(), "%s: 'To' is a directory", __FUNCTION__);
                err = fs_errorForPOSIXError(EISDIR);
                goto exit;
            }
        }

        // In case of directory rename - check that destination directory is empty
        if (toDirEntryData.type == FSItemTypeDirectory) {
            overDirItem = [DirItem dynamicCast:overFatItem];
            if (!overDirItem) {
                os_log_error(fskit_std_log(), "%s: could not cast to DirItem", __FUNCTION__);
                err = fs_errorForPOSIXError(EIO);
                goto exit;
            }

            err = [overDirItem checkIfEmpty];

            if (err) {
                os_log_error(fskit_std_log(), "%s: overDirItem checkIfEmpty %@", __FUNCTION__, err);
                goto exit;
            }
        }

        // Update directory version.
        dstDirItem.dirVersion++;

        // Mark all node directory entries as deleted
        err = [dstDirItem markDirEntriesAsDeletedAndUpdateMtime:overFatItem];

        if (err) {
            os_log_error(fskit_std_log(), "%s: unable to remove 'toItem' %@", __FUNCTION__, err);
            goto exit;
        }
        [_itemCache removeItem:overFatItem];

        if (dstNameCache) {
            [dstNameCache removeDirEntryNamed:(char *)destinationName.data.bytes
                                     ofLength:destinationName.data.length
                                  offsetInDir:overFatItem.entryData.firstEntryOffsetInDir];
        }

        /*
         * Release clusters only if the node is unlinked i.e a overItem wasn't
         * passed but the internal lookup above returned overFatItem.
         * o.w mark it as deleted for reclaim to handle freeing the clusters later.
         */
        if (overItem != NULL) {
            [overFatItem setDeleted];
        } else if (overFatItem.firstCluster != 0) {
            // TBD - rdar://115106714 (Invalidate the dir's metadata blocks from cache when removing dir item)
            // Tomer - potential plugin bug?
            [dstDirItem.volume.fatManager freeClusters:overFatItem.numberOfClusters
                                                ofItem:overFatItem
                                          replyHandler:^(NSError * _Nullable error) {
                err = error;
            }];

            if (err) {
                os_log_error(fskit_std_log(), "%s: unable to free toItem clusters %@", __FUNCTION__, err);
                goto exit;
            }
        }
    }

    // Perform the switch
    // write new entry in destination directory
    isFromSymlink = [SymLinkItem dynamicCast:sourceItem] ? true : false;
    /* Create dir entry in the destination directory. */
    [dstDirItem createNewDirEntryNamed:destinationName.string
                                  type:(isFromSymlink) ? FSItemTypeSymlink : sourceDirEntryData.type
                            attributes:sourceAttr
                      firstDataCluster:sourceFatItem.firstCluster
                          replyHandler:^(NSError * _Nullable error,
                                         uint64_t offsetInDir) {
        if (error) {
            err = error;
        } else {
            newEntryOffset = offsetInDir;
        }
    }];

    if (err) {
        os_log_error(fskit_std_log(), "%s: create new entry failed with error = %@.", __FUNCTION__, err);
        goto exit;
    }

    [_itemCache removeItem:sourceFatItem];

    // Update the necessary fields of source item
    // Get the new direcory entries
    [dstDirItem lookupDirEntryNamed:destinationName.string
                          dirNameCache:nil
                          lookupOffset:&newEntryOffset
                          replyHandler:^(NSError *error,
                                         DirEntryData *dirEntryData) {
        if (error) {
            err = error;
        } else {
            newDirEntryData = dirEntryData;
        }
    }];

    if (err) {
        os_log_error(fskit_std_log(), "%s: lookup new entry failed with error = %@.", __FUNCTION__, err);
        goto exit;
    }

    //Copy the new Name
    sourceFatItem.name = destinationName.string;
    // Switching dirNode pointer to toDirNode
    sourceFatItem.parentDir = dstDirItem;

    if (dstNameCache) {
        [dstNameCache insertDirEntryNamed:(char *)destinationName.data.bytes
                                 ofLength:destinationName.data.length
                              offsetInDir:newDirEntryData.firstEntryOffsetInDir];
        [self.nameCachePool doneWithNameCacheForDir:dstDirItem];
        dstNameCache = nil;
    }

    /*
     * WE ARE NOW COMMITTED TO THE RENAME -- IT CAN NO LONGER FAIL, UNLESS WE
     * UNWIND THE ENTIRE TRANSACTION.
     */
    [self.nameCachePool getNameCacheForDir:sourceDirItem
                                cachedOnly:true
                              replyHandler:^(NSError * _Nullable poolError, DirNameCache * _Nullable cache, bool isNew) {
        // We do not need to do anything if we did not get a name cache for any reason
        srcNameCache = cache;
    }];

    // remove old entry from source directory
    err = [sourceDirItem markDirEntriesAsDeletedAndUpdateMtime:sourceFatItem];
    if (err) {
        os_log_error(fskit_std_log(), "%s: unable to remove old file / directory entry %@", __FUNCTION__, err);
    }
    
    if (srcNameCache) {
        [srcNameCache removeDirEntryNamed:(char *)sourceName.data.bytes
                                 ofLength:sourceName.data.length
                              offsetInDir:sourceFatItem.entryData.firstEntryOffsetInDir];
        [self.nameCachePool doneWithNameCacheForDir:sourceDirItem];
    }

    sourceFatItem.entryData = newDirEntryData;

    /* Insert the new item to the items cache */
    [_itemCache insertItem:sourceFatItem replyHandler:^(FATItem * _Nullable cachedItem,
                                                        NSError * _Nullable error) {
        if (error) {
            err = error;
        }
        // TODO: Do we check if the cachedItem != sourceFatItem? If so, what do we do?
    }];

    // In case of directory rename - we need to update '..' with the new parent cluster
    if ((sourceDirEntryData.type == FSItemTypeDirectory) && (sourceDirectory != destinationDirectory)) {
        // Get the '..' entry of 'To' Directory
        dirFromItem = [DirItem dynamicCast:sourceFatItem];
        [dirFromItem lookupDirEntryNamed:@".."
                            dirNameCache:nil
                            lookupOffset:nil
                            replyHandler:^(NSError *error,
                                           DirEntryData *dirEntryData) {
            if (error) {
                err = error;
            } else {
                dotDotEntryData = dirEntryData;
            }
        }];
        
        if (err) {
            os_log_error(fskit_std_log(), "%s: unable to lookup .. %@", __FUNCTION__, err);
        } else {
            [dotDotEntryData setFirstCluster:dstDirItem.firstCluster
                              fileSystemInfo:self.systemInfo];
        }
    }

    //Update Directory version
    sourceDirItem.dirVersion++;
exit:
    if (dstNameCache) {
        [self.nameCachePool doneWithNameCacheForDir:dstDirItem];
    }
    return reply(nil, err);
}

- (void)setAttributes:(FSItemSetAttributesRequest *)newAttributes
               onItem:(FSItem *)item
         replyHandler:(void (^)(FSItemAttributes * _Nullable, NSError * _Nullable))reply
{
    FSItemGetAttributesRequest *getAttrsReq = [[FSItemGetAttributesRequest alloc] init];
    FATItem *theItem = [FATItem dynamicCast:item];
    FSItemAttributes *itemAttrs = nil;
    NSError *error = nil;

    if (theItem == nil) {
        os_log_error(fskit_std_log(), "%s: Received an invalid item", __func__);
    }

    /* Check we're not attempting to change read-only fields */
    if ([Utilities containsReadOnlyAttributes:newAttributes]) {
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    [self.fatManager setDirtyBitValue:true replyHandler:^(NSError * _Nullable error) {
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to set dirty bit", __func__);
        }
    }];

    error = [theItem setAttributes:newAttributes];
    if (error) {
        return reply(nil, error);
    }

    /* Initialize the getAttr bits */
    getAttrsReq.wantedAttributes = FSItemAttributeGID | FSItemAttributeUID |
                                   FSItemAttributeMode | FSItemAttributeSize |
                                   FSItemAttributeAllocSize | FSItemAttributeType |
                                   FSItemAttributeFileID | FSItemAttributeParentID |
                                   FSItemAttributeFlags | FSItemAttributeLinkCount |
                                   FSItemAttributeAccessTime | FSItemAttributeBirthTime |
                                   FSItemAttributeModifyTime | FSItemAttributeChangeTime;
    itemAttrs = [theItem getAttributes:getAttrsReq];
    return reply(itemAttrs, nil);
}

- (void)synchronizeWithReplyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    __block NSError *err;

    err = [self sync];
    if (err) {
        os_log_error(fskit_std_log(), "%s: sync failed, error %@", __func__, err);
    }

    /* If there are no open preallocated / unlinked files, clear dirty bit */
    if (([[self.preAllocatedOpenFiles objectAtIndex:0] unsignedLongLongValue] == 0) &&
        ([[self.openUnlinkedFiles objectAtIndex:0] unsignedLongLongValue] == 0)) {
        [self.fatManager setDirtyBitValue:dirtyBitClean
                             replyHandler:^(NSError * _Nullable error) {
            if (error) {
                return reply(error);
            }
        }];
    }

    [self.resource synchronousMetadataFlushWithReplyHandler:^(NSError * _Nullable error) {
        err = error;
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to flush meta cache, error %@",  __func__, error);
        }
    }];

    reply(err);
}

-(void)activateWithOptions:(FSTaskParameters *)options
              replyHandler:(void (^)(FSItem * _Nullable rootItem,
                                     NSError * _Nullable err))reply
{
    os_log_debug(fskit_std_log(), "%s:start", __FUNCTION__);
    [self FatMount:options replyHandler:^(FSItem * _Nullable rootItem,
                                          NSError * _Nullable error) {
        return reply(rootItem, error);
    }];
    _itemCache = [[ItemCache alloc] initWithVolume:self];
    os_log_debug(fskit_std_log(), "%s:end", __FUNCTION__);
}

-(void)deactivateWithOptions:(FSDeactivateOptions)options
                replyHandler:(void (^)(NSError * _Nullable err))reply
{
    os_log_debug(fskit_std_log(), "%s:start", __FUNCTION__);
    __block NSError *err;
    // Should we do this sync call or FSKit should take care of it?
    [self synchronizeWithReplyHandler:^(NSError * _Nullable error) {
        err = error;
    }];

    if (!err) {
        [self unmountWithReplyHandler:^{
            return;
        }];
    }
    os_log_debug(fskit_std_log(), "%s:end:%@", __FUNCTION__, err);
    return reply(nil);
}

- (void)mountWithOptions:(FSTaskParameters *)options
            replyHandler:(nonnull void (^)(FSItem * _Nullable rootItem,
                                           NSError * _Nullable err))reply
{
    [self FatMount:options replyHandler:^(FSItem * _Nullable rootItem,
                                          NSError * _Nullable error) {
        return reply(rootItem, error);
    }];
    _itemCache = [[ItemCache alloc] initWithVolume:self]; //TODO: do we keep both mount and activate?
}

- (void)unmountWithReplyHandler:(nonnull void (^)(void))reply
{
#if TARGET_OS_OSX
    /* For MacOS, flush writes to disk before unmounting */
    [self.resource synchronousMetadataFlushWithReplyHandler:^(NSError * _Nullable error) {
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to meta flush, error %@", __func__, error);
        }
    }];
#endif

    /* Verify FS is clean, log if dirty */
    [self.fatManager getDirtyBitValue:^(NSError * _Nullable error, dirtyBitValue value) {
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to read dirty bit value, error %@", __func__, error);
        } else if ((value == dirtyBitDirty) || (value == dirtyBitUnknown)) {
            os_log_error(fskit_std_log(), "%s: unmounting, file system state is %s", __func__, value == dirtyBitDirty ? "dirty" : "unknown");
        }
    }];

    return reply();
}

- (void)close:(nonnull FSItem *)item
  keepingMode:(int)mode
 replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP));
}

- (void)open:(nonnull FSItem *)item
    withMode:(int)mode
replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP));
}

-(void)blockmapFile:(FSItem *)item 
              range:(NSRange)theRange
            startIO:(BOOL)firstIO
              flags:(FSBlockmapFlags)flags
        operationID:(uint64_t)operationID
        usingPacker:(FSExtentPacker)packer
       replyHandler:(void (^)(NSError * _Nullable))reply
{
    FileItem *fileItem = [FileItem dynamicCast:item];

    if (!fileItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item.", __func__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    __block NSError *error = nil;

    [fileItem blockmapRange:theRange
                    startIO:firstIO
                      flags:flags
                operationID:operationID
                usingBlocks:packer
               replyHandler:^(NSError *blockmapError) {
        if (blockmapError) {
            os_log_error(fskit_std_log(), "%s: couldn't blockmap file. Error = %@.", __func__, blockmapError);
            error = blockmapError;
        }
    }];

    return reply(error);
}

- (void)endIO:(nonnull FSItem *)item
        range:(NSRange)range
       status:(NSError *)ioStatus
        flags:(FSBlockmapFlags)flags
  operationID:(uint64_t)operationID
 replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    FileItem *fileItem = [FileItem dynamicCast:item];

    if (!fileItem) {
        os_log_error(fskit_std_log(), "%s: got an invalid item.", __func__);
        return reply(fs_errorForPOSIXError(EINVAL));
    }

    NSError *error = [fileItem endIOOfRange:range
                                     status:ioStatus == nil ? 0 : (int)ioStatus.code
                                      flags:flags
                                operationID:operationID];
    return reply(error);
}


#pragma mark FATOps methods

-(DirItem *)createDirItemWithParent:(FATItem * _Nullable)parentDir
					   firstCluster:(uint32_t)firstCluster
					   dirEntryData:(DirEntryData * _Nullable)dirEntryData
							   name:(nonnull NSString *)name
							 isRoot:(bool)isRoot;
{
	return nil; // sub-classes should implement.
}

-(FileItem *)createFileItemWithParent:(FATItem * _Nullable)parentDir
						 firstCluster:(uint32_t)firstCluster
						 dirEntryData:(DirEntryData * _Nullable)dirEntryData
								 name:(nonnull NSString *)name
{
	return nil; // sub-classes should implement.
}

-(SymLinkItem *)createSymlinkItemWithParent:(FATItem * _Nullable)parentDir
							   firstCluster:(uint32_t)firstCluster
							   dirEntryData:(DirEntryData * _Nullable)dirEntryData
									   name:(nonnull NSString *)name
                              symlinkLength:(uint16_t)length
{
	return nil; // sub-classes should implement.
}

- (void)FatMount:(FSTaskParameters *)options
    replyHandler:(nonnull void (^)(FSItem * _Nullable, NSError * _Nullable))reply
{
    reply(nil, fs_errorForPOSIXError(ENOTSUP)); // sub-classes should implement.
}

- (void)getFreeClustersStats:(uint32_t * _Nonnull)freeClusters
                replyHandler:(nonnull void (^)(NSError * _Nullable))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP)); // sub-classes should implement.
}

-(void)calcNumOfDirEntriesForName:(struct unistr255)unistrName
                     replyHandler:(void (^)(NSError * _Nullable error, uint32_t numberOfEntries))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP), 0);
}

-(void)nameToUnistr:(NSString *)name
              flags:(uint32_t)flags
       replyHandler:(void (^)(NSError * _Nullable, struct unistr255))reply {
    struct unistr255 mock = {0};
    reply(fs_errorForPOSIXError(ENOTSUP), mock);
}

-(NSError *)verifyFileSize:(uint64_t)fileSize
{
	return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

-(bool)isOffsetInMetadataZone:(uint64_t)offset
{
    return false; // sub-classes should implement.
}

-(NSError *)sync
{
    return(fs_errorForPOSIXError(ENOTSUP)); // sub-classes should implement.
}

- (void)setVolumeLabel:(nonnull NSData *)name
               rootDir:(nonnull DirItem *)rootItem
          replyHandler:(nonnull void (^)(FSFileName * _Nullable,
                                         NSError * _Nullable))reply
{
    return reply(nil, (fs_errorForPOSIXError(ENOTSUP))); // sub-classes should implement.
}

- (void)setVolumeName:(FSFileName *)name
         replyHandler:(void (^)(FSFileName * newName,
                                NSError *error))reply
{
    __block FSFileName *volumeName = nil;
    __block NSError *error = nil;
    [self setVolumeLabel:name.data
                 rootDir:self.rootItem
            replyHandler:^(FSFileName * _Nullable newVolumeName,
                           NSError * _Nullable volumeLabelError) {
        error = volumeLabelError;
        if (!error) {
            volumeName = newVolumeName;
        }
    }];
    if (error) {
        return reply(nil, error);
    }
    return reply(volumeName, nil);
}

- (void)preallocate:(FSItem *)item
             offset:(uint64_t)offset
             length:(size_t)length
              flags:(FSPreallocateFlags)flags
        usingPacker:(FSExtentPacker)packer
       replyHandler:(void (^)(size_t bytesAllocated,
                              NSError * error))reply
{

    __block NSError* err = nil;

    __block uint64_t bytesAllocated = 0;

    /* Cannot change size of a directory or symlink! */
    FileItem *fileItem = [FileItem dynamicCast:item];
    if (!fileItem) {
        os_log_error(fskit_std_log(), "%s: Cannot change size of a directory or symlink", __FUNCTION__);
        return reply(0, fs_errorForPOSIXError(EPERM));
    }

    if (flags & FSPreallocateFromVol) {
        os_log_error(fskit_std_log(), "%s: Not supporting FSPreallocateFromVol mode", __FUNCTION__);
        return reply(0, fs_errorForPOSIXError(ENOTSUP));
    }

    if (offset != 0) {
        /*
         * Offset relative to EOF, this is the only valid value in
         * FSPreallocateFromEOF, which is the only preallocation
         * method we support.
         */
        os_log_error(fskit_std_log(), "%s: offset given wasn't 0 - %lld", __FUNCTION__, offset);
        return reply(0, fs_errorForPOSIXError(EINVAL));
    }

    [self.fatManager setDirtyBitValue:dirtyBitDirty
                         replyHandler:^(NSError * _Nullable fatError) {
        if (fatError) {
            /* Log the error, keep going */
            os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __FUNCTION__, fatError);
        }
    }];

    uint64_t curAllocatedSize = fileItem.numberOfClusters * self.systemInfo.bytesPerCluster;

    /*
     * Always ignore the FSPreallocateContig flag and do best effort. The volume cluster allocation algorithm
     * will always try to truncate the file contiguously from its last cluster, but only if it is possible.
     */
    [fileItem preallocate:length
             allowPartial:!(flags & FSPreallocateAll)
             mustBeContig:false
             replyHandler:^(NSError * _Nullable error, uint64_t AllocatedBytes) {
        err = error;
        bytesAllocated = AllocatedBytes;
    }];

    if (!err) {
        [fileItem fetchFileExtentsFrom:curAllocatedSize
                                    to:(length + curAllocatedSize)
                       lastValidOffset:curAllocatedSize
                           usingBlocks:packer
                          replyHandler:^(NSError * _Nullable fetchError) {
            err = fetchError;
        }];
    }

    return reply(bytesAllocated, err);
}

@end

/*
 * Filesystem-dependent operations
 * Some methods are different for different types of FAT (12, 16 and 32 bit).
 * To make the main code clearer, we'll handle the different types internally
 * here rather than use function pointers.
 * ExFAT is expected to be similar to FAT32 as it also uses 32b for block
 * numbers.
 */

@implementation FSOperations

-(id)initWithType:(fatType)type
{
    self = [super init];
    _fatType = type;
    return self;
}

-(uint32_t)numBytesPerClusterInFat
{
    switch(_fatType) {
        case FAT12:
            /*
             * In FAT12, this number is not a constant, but a function
             * of the cluster number.  Therefore, returning 0, so the
             * caller would call fatEntryOffsetForCluster.
             */
            return 0;
        case FAT16:
            return sizeof(uint16_t);
        default:
            return sizeof(uint32_t);
    }
}

- (uint32_t)fatEntryOffsetForCluster:(uint32_t)cluster
{
    switch(_fatType) {
        case FAT12:
            /* FAT12 tightly packs 12 bits per cluster -> 3 bytes for 2 clusters */
            return (cluster * 3) / 2;
        case FAT16:
            return cluster * sizeof(uint16_t);
        default:
            return cluster * sizeof(uint32_t);
    }
}

- (uint32_t)getNextClusterFromEntryForCluster:(uint32_t)cluster
                                        entry:(uint8_t *)entry
{
    uint32_t result = 0;
    switch(_fatType) {
        case FAT12:
            /*
             * Entry may not be 2B aligned, fetch the value byte-by-byte:
             * If the cluster number is even, we want the low 12 bits. If the
             * cluster number is odd, we want the high 12 bits
             */
            result = entry[0] | (entry[1] << 8);
            if (cluster & 1) {
                result >>= 4;
            } else {
                result &= 0x0FFF;
            }
            return result;
        case FAT16:
            result = getuint16(entry);
            return result;
        default:
            result = getuint32(entry);
            return result & 0x0FFFFFFFU;
    }
}

- (uint32_t)setFatEntryForCluster:(uint32_t)cluster
                            entry:(uint8_t *)entry
                        withValue:(uint32_t)value
{
    uint32_t oldValue = 0;
    uint32_t newValue = 0;

    switch(_fatType) {
        case FAT12:
            oldValue = (entry[0]) | (entry[1] << 8);
            if (cluster & 1) {
                newValue = (oldValue & 0x000F) | (value << 4);
                entry[0] = newValue;
                entry[1] = newValue >> 8;
                oldValue >>= 4;
            } else {
                newValue = (oldValue & 0xF000) | (value & 0x0FFF);
                entry[0] = newValue;
                entry[1] = newValue >> 8;
                oldValue &= 0x0FFF;
            }
            return oldValue;
        case FAT16:
            oldValue = getuint16(entry);
            putuint16(entry, value);
            return oldValue;
        default:
            oldValue = getuint32(entry);
            putuint32(entry, (value & 0x0FFFFFFFU) | (oldValue & 0xF0000000U) );
            return oldValue & 0x0FFFFFFFU;
    }
}

-(uint32_t)getDirtyBitCluster
{
    switch(self.fatType) {
        case FAT16:
        case FAT32:
            return FAT_DIRTY_BIT_CLUSTER;
        default:
            return 0; // Shouldn't be called for FAT12, as there's no dirty bit in FAT12.
    }
}

-(uint32_t)getDirtyBitMask
{
    switch(self.fatType) {
        case FAT16:
            return (1 << FAT_16_DIRTY_BIT_IDX);
        case FAT32:
            return (1 << FAT_32_DIRTY_BIT_IDX);
        default:
            return 0; // Shouldn't be called for FAT12, as there's no dirty bit in FAT12.
    }
}

-(dirtyBitValue)getDirtyBitValueForEntry:(uint8_t *)dirtyBitEntry
{
    uint32_t dirtyBitEntryValue = [self getNextClusterFromEntryForCluster:[self getDirtyBitCluster]
                                                                    entry:dirtyBitEntry];

    /* Figure out which bit in the FAT entry is the dirty bit. */
    uint32_t dirtyBitMask = [self getDirtyBitMask];

    switch(self.fatType) {
        case FAT16:
        case FAT32:
            if (dirtyBitEntryValue & dirtyBitMask) {
                /* In msdos, the 'dirty' value of the dirty bit is 0. */
                return dirtyBitClean;
            } else {
                return dirtyBitDirty;
            }
            break;
        default:
            return dirtyBitUnknown; // Shouldn't be called for FAT12, as there's no dirty bit in FAT12.
    }
}

-(void)applyDirtyBitValueToEntry:(uint8_t *)dirtyBitEntry
                        newValue:(dirtyBitValue)newValue
{
    uint32_t dirtyBitEntryValue = [self getNextClusterFromEntryForCluster:[self getDirtyBitCluster]
                                                                    entry:dirtyBitEntry];

    /* Figure out which bit in the FAT entry is the dirty bit. */
    uint32_t dirtyBitMask = [self getDirtyBitMask];

    switch(self.fatType) {
        case FAT16:
        case FAT32:
            if (newValue == dirtyBitDirty) {
                /* In msdos, the 'dirty' value of the dirty bit is 0,
                 so we clear the relevant bit in case we want to mark it as 'dirty'. */
                dirtyBitEntryValue &= ~dirtyBitMask;
            } else {
                dirtyBitEntryValue |= dirtyBitMask;
            }
            break;
        default:
            return; // Shouldn't be called for FAT12, as there's no dirty bit in FAT12.
    }

    [self setFatEntryForCluster:[self getDirtyBitCluster]
                          entry:dirtyBitEntry
                      withValue:dirtyBitEntryValue];
}

@end
