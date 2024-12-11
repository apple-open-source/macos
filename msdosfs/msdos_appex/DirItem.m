/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "ExtensionCommon.h"
#import "DirNameCache.h"
#import "FATManager.h"
#import "FATVolume.h"
#import "DirBlock.h"
#import "DirItem.h"
#import "utils.h"

@implementation DirItem

- (instancetype)initInVolume:(FATVolume *)volume
					   inDir:(FATItem * _Nullable)parentDir
				  startingAt:(uint32_t)firstCluster
					withData:(DirEntryData * _Nullable)entryData
					 andName:(nonnull NSString *)name
					  isRoot:(bool)isRoot
{
	self = [super initInVolume:volume
						 inDir:parentDir
					startingAt:firstCluster
					  withData:entryData
					   andName:name
						isRoot:isRoot];
	if (self != nil) {
		self.isRoot = isRoot;
		self.dirVersion = 1;
		self.maxRADirBlock = 0;
		self.offsetForNewEntry = 0;
        self.sem = dispatch_semaphore_create(1);
	}

	return self;
}

#pragma mark DirOps operations (sub-classes should implement)

-(uint64_t)getDirBlockSize
{
    return 0; // sub-classes should implement.
}

-(uint64_t)getDirSize
{
    return 0; // sub-classes should implement.
}

-(uint32_t)dirEntrySize
{
    return 0; // sub-classes should implement.
}

-(NSError *)createDotEntriesWithAttrs:(FSItemAttributes *)mkdirAttrs
{
    // Sub-classes should implement this method in case they need to
    // create '.' and '..' dir entries when we create new directories.
    return nil;
}

-(NSError *)updateModificationTimeOnCreateRemove
{
    // Sub-classes should implement this method in case they need to update the
    // modification time of the directory when we create/remove items in it.
    return nil;
}

-(NSError *)updateDotDirEntryTimes:(FSItemSetAttributesRequest *)attrs
{
    // Sub-classes should implement this method in case they need to update the
    // modification time of the dot dir entry of the directory.
    return nil;
}

-(void)iterateFromOffset:(uint64_t)startOffsetInDir
				 options:(iterateDirOptions)options
            replyHandler:(iterateDirStatus (^)(NSError *error,
											   dirEntryType result,
											   uint64_t dirEntryOffset,
                                               struct unistr255 * _Nullable name,
											   DirEntryData * _Nullable dirEntryData))reply
{
	reply(fs_errorForPOSIXError(ENOTSUP), FATDirEntryUnknown, 0, nil, nil); // sub-classes should implement.
}

-(NSError *)writeDirEntriesToDisk:(DirEntryData *)dirEntryData
                         atOffset:(uint64_t)startOffsetInDir
                             name:(struct unistr255)name
                  numberOfEntries:(uint32_t)numberOfEntries
{
    return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

-(NSError *)markDirEntriesAsDeleted:(FATItem *)forItem
{
    return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

-(NSError *)writeDirEntryDataToDisk:(DirEntryData *)dirEntryData
{
    return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

-(void)createEntrySetForName:(struct unistr255)unistrName
                    itemType:(FSItemType)itemType
                firstCluster:(uint32_t)firstCluster
                       attrs:(FSItemAttributes *)attrs
                 offsetInDir:(uint64_t)offsetInDir
                      hidden:(bool)hidden
                replyHandler:(void (^)(NSError * _Nullable,
                                       DirEntryData * _Nullable))reply
{
    return reply(fs_errorForPOSIXError(ENOTSUP), nil); // sub-classes should implement.
}

-(NSError *)verifyCookieOffset:(uint32_t)offset
{
    return fs_errorForPOSIXError(ENOTSUP); // sub-classes should implement.
}

#pragma mark Common operations

-(bool)isFat1216RootDir
{
    return (self.isRoot && [self.volume.systemInfo isFAT12Or16]);
}

-(void)lookupDirEntryNamed:(NSString *)lookupName
			  dirNameCache:(DirNameCache * _Nullable)nameCache
			  lookupOffset:(uint64_t * _Nullable)lookupOffset
              replyHandler:(void (^)(NSError *error, DirEntryData *dirEntryData))reply
{
	__block uint64_t        dirEntryOffsetInCache   = 0;
	__block bool            foundDirEntryInCache    = false;
    __block bool            foundDirEntry           = false;
    __block NSError         *iterateDirError        = nil;
    __block DirEntryData    *dirEntryDataFound      = nil;
    uint64_t                iterateDirStartOffset   = 0;
    __block struct unistr255 unistrName = {0};

    if (lookupName == nil) {
        return reply(fs_errorForPOSIXError(EINVAL), nil);
    }

    [self.volume nameToUnistr:lookupName
                        flags:[Utilities isDotOrDotDot:(char *)lookupName.UTF8String length:lookupName.length] ? 0 : UTF_SFM_CONVERSIONS
                 replyHandler:^(NSError * _Nonnull convertError, struct unistr255 convertedName) {
        if (convertError) {
            iterateDirError = convertError;
        } else {
            unistrName = convertedName;
        }
    }];

    if (iterateDirError) {
        return reply(iterateDirError, nil);
    }

    CONV_Unistr255ToLowerCase(&unistrName);

    if (lookupOffset) {
        iterateDirStartOffset = *lookupOffset;
    } else if (nameCache) {
        [nameCache lookupDirEntryNamedUtf16:&unistrName
                               replyHandler:^(NSError *error, uint64_t entryOffsetInDir) {
            if (!error) {
                dirEntryOffsetInCache = entryOffsetInDir;
                foundDirEntryInCache = true;
            } else if (error.code == ENOENT) {
                if (!nameCache.isIncomplete) {
                    /* If the cache is complete and we didn't find the requested name, it doesn't exist. */
                    iterateDirError = error;
                }
            } else {
                iterateDirError = error;
            }
        }];
    }

    if (iterateDirError) {
        return reply(iterateDirError, nil);;
    }

	if (foundDirEntryInCache) {
        // Check if the hint we got from the cache is what we are looking for. If not
        // just do full lookup in the dir from offset 0.
        [self iterateFromOffset:dirEntryOffsetInCache
                        options:0
                   replyHandler:^iterateDirStatus(NSError * _Nonnull error,
                                                  dirEntryType result,
                                                  uint64_t dirEntryOffset,
                                                  struct unistr255 * name,
                                                  DirEntryData * _Nullable dirEntryData) {
            if (error) {
                os_log_error(fskit_std_log(), "%s: iterate dir failed with error = %@.", __FUNCTION__, error);
                return iterateDirStop;
            }
            if (result == FATDirEntryFound) {
                /* Convert to lower case */
                if (unistrName.length == name->length)
                {
                    CONV_Unistr255ToLowerCase(name);
                    if (!memcmp(name->chars, unistrName.chars, unistrName.length*sizeof(unistrName.chars[0])))
                    {
                        foundDirEntry = true;
                        dirEntryDataFound = dirEntryData;
                        return iterateDirStop;
                    }
                }
            }
            return iterateDirStop;
        }];
	}

    if (foundDirEntry) {
        return reply(iterateDirError, dirEntryDataFound);
    }

	// We've reached here if one of these cases applies:
	// 1. We've found this dir entry in cache.
	// 2. We didn't find this dir entry in cache, but the cache is incomplete. So we should iterate the directory.
    // 3. We got a non-null lookupOffset (hint).
	[self iterateFromOffset:iterateDirStartOffset
					options:0
               replyHandler:^iterateDirStatus(NSError * _Nonnull error,
											  dirEntryType result,
											  uint64_t dirEntryOffset,
                                              struct unistr255 * name,
											  DirEntryData * _Nullable dirEntryData) {
		if (error) {
            os_log_error(fskit_std_log(), "%s: iterate dir failed with error = %@.", __FUNCTION__, error);
			iterateDirError = error;
			return iterateDirStop;
		}
		if (result == FATDirEntryFound) {
            /* Convert to lower case */
            if (unistrName.length == name->length)
            {
                CONV_Unistr255ToLowerCase(name);
                if (!memcmp(name->chars, unistrName.chars, unistrName.length*sizeof(unistrName.chars[0])))
                {
                    foundDirEntry = true;
                    dirEntryDataFound = dirEntryData;
                    return iterateDirStop;
                }
            }
        }
		if (lookupOffset) {
			/* In case we got a hint to a specific offset, we expect to find the entry on the first iteration. */
			os_log_fault(fskit_std_log(), "%s: got a wrong offset from hint (%llu).", __FUNCTION__, *lookupOffset);
            iterateDirError = fs_errorForPOSIXError(EFAULT);
			return iterateDirStop;
		}
        if (result == FATDirEntryEmpty) {
            /* There are no dir entries after an empty one. */
            return iterateDirStop;
        }
		return iterateDirContinue;
	}];

	if (!foundDirEntry && !iterateDirError) {
		/* Couldn't find dir entry. */
        iterateDirError = fs_errorForPOSIXError(ENOENT);
	}
    return reply(iterateDirError, dirEntryDataFound);
}

-(NSError * _Nullable)fillNameCache:(DirNameCache *)nameCache
{
	__block NSError *blockError = nil;

	[self iterateFromOffset:0
					options:0
               replyHandler:^iterateDirStatus(NSError * _Nonnull error,
											  dirEntryType result,
											  uint64_t dirEntryOffset,
                                              struct unistr255 * name,
											  DirEntryData * _Nullable dirEntryData) {
		if (error) {
			os_log_error(fskit_std_log(), "%s: iterate dir failed with error = %@.", __FUNCTION__, error);
			blockError = error;
			return iterateDirStop;
		}
		if (result == FATDirEntryFound) {
			blockError = [nameCache insertDirEntryNamedUtf16:name
                                                 offsetInDir:dirEntryOffset];
			if (blockError) {
				os_log_error(fskit_std_log(), "%s: insert dir entry to name cache failed with error = %@.", __FUNCTION__, error);
				return iterateDirStop;
			}
        } else if (result == FATDirEntryEmpty) {
            /* There are no dir entries after an empty one. */
            return iterateDirStop;
        }
		return iterateDirContinue;
	}];
	if (blockError) {
		nameCache.isIncomplete = true;
	}
	return blockError;
}

/**
 Find contiguous available dir entry slots in the directory. Allocates additional dir clusters if needed.
 @param numEntries number of entries needed
 @param useOffsetHint Whether or not the offsetHint is valid.
 @param offsetHint An optional hint for where is the first available dir entry.
 @param reply in case of success, the reply block will be called with:
    firstEntryOffset: The offset to the start of the found group of dir entries.
    areThereHolesInDir: Whether or not we've encountered deleted entries while we've iterated over the directory.
    In case of an error, returns reply(error, 0, true).
 */
-(void)findFreeSlots:(uint32_t)numEntries
	   useOffsetHint:(bool)useOffsetHint
		  offsetHint:(uint64_t)offsetHint
        replyHandler:(void (^)(NSError *error,
							   uint64_t firstEntryOffset,
							   bool areThereHolesInDir))reply
{
	__block NSError *error = nil;
	__block uint32_t numFreeEntriesFound = 0;
	__block uint64_t firstEntryFoundOffset = 0;
	__block bool areThereHolesInDir = false; // We assume that we don't have holes as long as we didn't find any.

    if ((!useOffsetHint) || (offsetHint < [self getDirSize])) {

        [self iterateFromOffset:(useOffsetHint ? offsetHint : 0)
                        options:0
                   replyHandler:^iterateDirStatus(NSError * _Nonnull iterateDirError,
                                                  dirEntryType result,
                                                  uint64_t dirEntryOffset,
                                                  struct unistr255 * _Nullable name,
                                                  DirEntryData * _Nullable dirEntryData) {
            if (iterateDirError) {
                os_log_error(fskit_std_log(), "%s: iterate dir failed with error = %@.", __func__, error);
                error = iterateDirError;
                return iterateDirStop;
            }

            if (result == FATDirEntryDeleted) {
                areThereHolesInDir = true;
                if (numFreeEntriesFound == 0) {
                    firstEntryFoundOffset = dirEntryOffset;
                }
                numFreeEntriesFound++;
                if (numFreeEntriesFound == numEntries) {
                    return iterateDirStop;
                }
                return iterateDirContinue;
            } else if (result == FATDirEntryEmpty) {
                if (numFreeEntriesFound == 0) {
                    firstEntryFoundOffset = dirEntryOffset;
                }
                numFreeEntriesFound++;
                return iterateDirStop;
            } else {
                /* Found a non-deleted/non-empty dir entry. Reset counters and continue. */
                numFreeEntriesFound = 0;
                firstEntryFoundOffset = 0;
                return iterateDirContinue;
            }
        }];
        
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed iterating the directory. Error = %@.", __func__, error);
            return reply(error, 0, true);
        }
    }

	if (numFreeEntriesFound == numEntries) {
        /* We have found enough deleted entries. */
		return reply(nil, firstEntryFoundOffset, areThereHolesInDir);
	}

    /* We may have found some deleted entries, but not enough.
     * We either got here because we've found an empty entry (= effectively EOD),
     * or we're done iterating the directory (the directory is entirely full with entries).
     * Need to check if we have enough allocated space.
     */
	uint32_t numOfAvailableEntries = 0;
	uint64_t clusterSize = self.volume.systemInfo.bytesPerCluster;
    uint64_t dirSize = [self getDirSize];

	if (numFreeEntriesFound > 0) {
		numOfAvailableEntries = (uint32_t)((dirSize - firstEntryFoundOffset)/ [self dirEntrySize]);
		if (numOfAvailableEntries >= numEntries) {
            /* We have enough allocated space. */
			return reply(nil, firstEntryFoundOffset, areThereHolesInDir);
		}
	}

	/* We need to allocate one or more clusters for the directory */

    if ([self isFat1216RootDir]) {
        /* In FAT12/16, the root directory cannot be extended. */
        os_log_error(fskit_std_log(), "%s: Can't extend FAT12/16 root directory.", __func__);
        return reply(fs_errorForPOSIXError(ENOSPC), 0, true);
    }

	uint32_t availableSpaceNeeded = (numEntries - numOfAvailableEntries) * [self dirEntrySize];
	uint32_t numClusterNeeded = (uint32_t)(roundup(availableSpaceNeeded, clusterSize) / clusterSize);

    __block uint32_t firstClusterToClear = 0;
    __block uint32_t numClustersToClear = 0;
	[self.volume.fatManager allocateClusters:numClusterNeeded
                                     forItem:self
                                allowPartial:false
                                mustBeContig:false
                                    zeroFill:false
                                replyHandler:^(NSError * _Nullable allocationError,
                                               uint32_t firstAllocatedCluster,
                                               uint32_t lastAllocatedCluster,
                                               uint32_t numAllocated) {
		if (allocationError) {
			error = allocationError;
		} else {
			/* Allocation succeeded. */
			if (numFreeEntriesFound == 0) {
				firstEntryFoundOffset = dirSize;
            }
            firstClusterToClear = firstAllocatedCluster;
            numClustersToClear = numAllocated;
		}
	}];
	if (error) {
		os_log_error(fskit_std_log(), "%s: Failed to allocate clusters. Error = %@.", __func__, error);
		return reply(error, 0, true);
	}

    /* zero-fill the newly allocated clusters. */
    error = [self.volume clearNewDirClustersFrom:firstClusterToClear
                                          amount:numClustersToClear];
    if (error) {
		os_log_error(fskit_std_log(), "%s: Failed to zero-fill clusters. Error = %@.", __func__, error);
		return reply(error, 0, true);
	}

	return reply(nil, firstEntryFoundOffset, areThereHolesInDir);
}

-(void)createNewDirEntryNamed:(NSString *)name
						 type:(FSItemType)type
				   attributes:(FSItemAttributes *)attrs
			 firstDataCluster:(uint32_t)firstDataCluster
                 replyHandler:(void (^)(NSError * _Nullable error,
                                        uint64_t offsetInDir))reply;
{
	__block NSError *error = nil;

    if (![attrs isValid:FSItemAttributeMode] && (type != FSItemTypeDirectory)) {
		os_log_error(fskit_std_log(), "%s: Trying to create a file/symlink without a valid mode.", __func__);
		return reply(fs_errorForPOSIXError(EINVAL), 0);
	}

	// TODO: enable this check? (where should we get READ_ONLY_FA_FIELDS from?)
//	if ( ( attrs->fa_validmask & READ_ONLY_FA_FIELDS ) /*|| ( attrs->fa_validmask & ~VALID_IN_ATTR_MASK )*/ )
//	{
//		return reply(fs_errorForPOSIXError(EINVAL), 0);
//	}

	if (firstDataCluster != 0 && ![self.volume.systemInfo isClusterValid:firstDataCluster]) {
		os_log_error(fskit_std_log(), "%s: got an invalid first cluster (%u).", __func__, firstDataCluster);
		return reply(fs_errorForPOSIXError(EINVAL), 0);
	}

	// Convert the search name to UTF-16
	__block struct unistr255 unistrName = {0};
	[self.volume nameToUnistr:name
						flags:0
                 replyHandler:^(NSError * _Nonnull convertError, struct unistr255 convertedName) {
		if (convertError) {
			error = convertError;
		} else {
			unistrName = convertedName;
		}
	}];
	if (error) {
		os_log_error(fskit_std_log(), "%s: Couldn't convert name to unistr. Error = %@.", __func__, error);
		return reply(error, 0);
	}

	// Calculate the amount of entries needed for this node.
	__block uint32_t numOfDirEntries = 0;
	[self.volume calcNumOfDirEntriesForName:unistrName
                               replyHandler:^(NSError * _Nullable calcError,
											  uint32_t numberOfEntries) {
		if (calcError) {
			error = calcError;
		} else {
			numOfDirEntries = numberOfEntries;
		}
	}];
	if (error) {
		os_log_error(fskit_std_log(), "%s: Couldn't calculate number of dir entries. Error = %@.", __func__, error);
		return reply(error, 0);
	}

	// Find an offset in the directory which can hold (contiguously) the needed amount of entries.
    __block uint64_t startOffset = 0;
	__block bool foundHolesInDir = false;
	[self findFreeSlots:numOfDirEntries
		  useOffsetHint:true
			 offsetHint:self.offsetForNewEntry
           replyHandler:^(NSError *findError,
						  uint64_t firstEntryOffset,
						  bool areThereHolesInDir) {
		if (findError) {
			error = findError;
		} else {
			startOffset = firstEntryOffset;
			foundHolesInDir = areThereHolesInDir;
		}
	}];
	if (error) {
		return reply(error, 0);
	}

    // If the file name starts with ".", make it invisible on Windows.
    bool hidden = (name.UTF8String[0] == '.');
	// Create and write the new dir entries to disk.
    __block DirEntryData *newEntryData = nil;
    [self createEntrySetForName:unistrName
                       itemType:type
                   firstCluster:firstDataCluster
                          attrs:attrs
                    offsetInDir:startOffset
                         hidden:hidden
                   replyHandler:^(NSError * _Nullable entrySetError,
                                  DirEntryData * _Nullable dirEntryData) {
        if (entrySetError) {
            error = entrySetError;
        } else {
            newEntryData = dirEntryData;
        }
    }];
	if (error) {
		os_log_error(fskit_std_log(), "%s: Couldn't create dir entry-set. Error = %@.", __func__, error);
		return reply(error, 0);
	}

    error = [self writeDirEntriesToDisk:newEntryData
                               atOffset:startOffset
                                   name:unistrName
                        numberOfEntries:numOfDirEntries];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Couldn't write new dir entries to disk. Error = %@.", __func__, error);
        return reply(error, 0);
    }

	/* If we got to this point we cannot fail, as we've already written the dir entries to disk. */

	if (foundHolesInDir) {
		/* There are holes in dir - zero offsetForNewEntry so future inserts to the directory will look for holes (the hard way) */
		self.offsetForNewEntry = 0;
	} else {
		/* There aren't holes in dir - update offsetForNewEntry */
		self.offsetForNewEntry = startOffset + (numOfDirEntries * [self dirEntrySize]);
	}

	// Update Directory version
	self.dirVersion++;

	return reply(nil, startOffset);
}

-(void)iterateDirEntriesAtOffset:(uint64_t)startOffsetInDir
                      numEntries:(uint32_t)numberOfEntries
               shouldWriteToDisk:(bool)writeToDisk
                    replyHandler:(void (^)(NSError * _Nullable error, void * _Nullable dirEntry))reply
{
    NSError *error = nil;
    size_t dirBlockSize = [self getDirBlockSize];
    uint64_t currentOffsetInDir = startOffsetInDir;
    uint32_t currentDirBlock = (uint32_t)(currentOffsetInDir / dirBlockSize);
    bool isFirstEntryInDirBlock = true; // Not needed if we decide not to do sub-block writes

    DirBlock *dirBlock = [[DirBlock alloc] initInDir:self];
    if (!dirBlock) {
        return reply(fs_errorForPOSIXError(ENOMEM), nil);
    }

    error = [dirBlock readRelativeDirBlockNum:currentDirBlock];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Couldn't read dir block at idx %u. Error = %@.", __func__, currentDirBlock, error);
        [dirBlock releaseBlock];
        return reply(error, nil);
    }

    for (uint32_t entryIdx = 0; entryIdx < numberOfEntries; entryIdx++) {
        reply(nil, [dirBlock getBytesAtOffset:currentOffsetInDir % dirBlockSize]);

        if (isFirstEntryInDirBlock) {
            isFirstEntryInDirBlock = false;
        }

        currentOffsetInDir += [self dirEntrySize];

        /* If we need to replace a dir block, or we finished updating all entries - perform a write */
        if (((currentOffsetInDir % dirBlockSize) == 0) || (entryIdx == numberOfEntries - 1)) {
            // Make sure offset is not in metadata zone
            if ([self.volume isOffsetInMetadataZone:dirBlock.offsetInVolume]) {
                os_log_error(fskit_std_log(), "%s: Dir offset (%llu) is within the metadata zone.", __func__, dirBlock.offsetInVolume);
                return reply(fs_errorForPOSIXError(EFAULT), nil);
            }

            // TODO: enable sub-block writes in macOS? (rdar://115047763)
            //#if TARGET_OS_OSX
            //            // for macOS, we only want to flush the relevant sector(s) for the given file's dir entries.
            //            // Notes:
            //            // 1. "endOffset" is using the already-incremented offset - that's on purpose,
            //            //    because in case we're at the beginning of a sector, we want the round-up
            //            //    operation to bring us to the end of the sector.
            //            // 2. In case the given file's dir entries are split into two dir blocks, we flush the first
            //            //    dir block's entries, then switch dir blocks below, then flush the second dir block's entries.
            //            uint64_t sectorSize     = self.volume.systemInfo.bytesPerSector;
            //            uint64_t startOffset    = ROUND_DOWN(firstEntryInDirBlockOffset, sectorSize);
            //            uint64_t endOffset      = ROUND_UP(currentOffsetInDir, sectorSize);
            //            uint32_t writeSize      = (uint32_t)(endOffset - startOffset);
            //            uint64_t writeOffset    = dirBlock.offsetInVolume + (startOffset % dirBlockSize);
            //            void*    pvSectorData   = (void *)((uint8_t *)dirBlock.data.bytes + (startOffset % dirBlockSize));
            //            iError = metaWriteSubBlock(psFSRecord, dirBlock->uAbsoluteDirBlockOffset, dirBlockSize, pvSectorData, writeOffset, writeSize);
            //#else
            // For iOS we don't use subblock writes as it's only relevant when doing delayed writes.
            error = [dirBlock writeToDisk];
            //#endif /* TARGET_OS_OSX */
            if (error) {
                os_log_error(fskit_std_log(), "%s: Failed to write the updated entries into the device. Error = %@.", __func__, error);
                [dirBlock releaseBlock];
                return reply(error, nil);
            }

            if (((currentOffsetInDir % dirBlockSize) == 0) && (entryIdx != numberOfEntries - 1)) {
                /* Continue to the next dir block. */
                currentDirBlock++;
                isFirstEntryInDirBlock = true;
                // Read dir block to memory.
                error = [dirBlock readRelativeDirBlockNum:currentDirBlock];
                if (error) {
                    os_log_error(fskit_std_log(), "%s: Couldn't read dir block idx (%u). Error = %@.", __func__, currentDirBlock, error);
                    [dirBlock releaseBlock];
                    return reply(error, nil);
                }
            }
        }
    }
    [dirBlock releaseBlock];
}

-(NSError *)markDirEntriesAsDeletedAndUpdateMtime:(FATItem *)forItem
{
    NSError *err = [self markDirEntriesAsDeleted:forItem];

    if (err) {
        os_log_error(fskit_std_log(), "%s: fail to mark enries as deleted", __FUNCTION__);
        return err;
    }

    // The directory now must have holes in it - set the offset for a new entry to 0
    // so future inserts to the directory will look for holes (the hard way).
    self.offsetForNewEntry = 0;

    /* Update parent dir modification time if needed. */
    err = [self updateModificationTimeOnCreateRemove];
    if (err) {
        /* In case the update failed, we still marked the entries as deleted so we silent the error */
        os_log_error(fskit_std_log(), "%s: update parent dir modification time failed with error = %@.", __FUNCTION__, err);
        err = nil;
    }

    return err;
}

-(NSError *)checkIfEmpty
{
    __block NSError* err = nil;

    [self iterateFromOffset:0
                    options:0
               replyHandler:^iterateDirStatus(NSError * _Nonnull error,
                                              dirEntryType result,
                                              uint64_t dirEntryOffset,
                                              struct unistr255 * _Nullable name,
                                              DirEntryData * _Nullable dirEntryData) {
        if (error) {
            os_log_error(fskit_std_log(), "%s: iterate dir failed with error = %@.", __FUNCTION__, error);
            err = error;
            return iterateDirStop;
        }

        if (result == FATDirEntryFound) {
            if (name->length > 2) {
                err = fs_errorForPOSIXError(ENOTEMPTY);
                return iterateDirStop;
            }
            NSString *nameString = nil;
            char utf8Name[FAT_MAX_FILENAME_UTF8] = {0};
            CONV_Unistr255ToUTF8(name, utf8Name);
            nameString = [NSString stringWithUTF8String:utf8Name];

            if ([Utilities isDotOrDotDot:utf8Name length:nameString.length] == false) {
                err = fs_errorForPOSIXError(ENOTEMPTY);
                return iterateDirStop;
            }
        }

        if (result == FATDirEntryEmpty) {
            /* There are no dir entries after an empty one. */
            return iterateDirStop;
        }

        return iterateDirContinue;
    }];

    return err;
}

-(void)getDirEntryOffsetByIndex:(uint32_t)index
                   replyHandler:(void (^)(NSError * _Nullable error, uint64_t offset, bool reachedEOF))reply
{
    __block bool reachedEOF = false;
    __block uint64_t offset = 0;
    __block NSError *err = nil;
    __block uint32_t currentIndex = 0;

    if (index > 0) {
        [self iterateFromOffset:0
                        options:0
                   replyHandler:^iterateDirStatus(NSError * _Nonnull error,
                                                  dirEntryType result,
                                                  uint64_t dirEntryOffset,
                                                  struct unistr255 * _Nullable name,
                                                  DirEntryData * _Nullable dirEntryData) {
            if (error) {
                err = error;
                os_log_error(fskit_std_log(), "%s iterateFromOffset error %d.\n", __FUNCTION__, (int)error.code);
                return iterateDirStop;
            };

            if (result == FATDirEntryFound) {
                if (currentIndex == index) {
                    offset = dirEntryOffset;
                    return iterateDirStop;
                }
                currentIndex++;
                return iterateDirContinue;
            } else if (result == FATDirEntryEmpty) {
                reachedEOF = true;
                return iterateDirStop;
            }

            return iterateDirContinue;
        }];
    }

    return reply(err, offset, reachedEOF);
}

-(void)purgeMetaBlocksFromCache:(void(^)(NSError * _Nullable))reply
{
    NSMutableArray<FSMetadataBlockRange *> *rangesToPurge = [NSMutableArray array];
    size_t clusterSize = self.volume.systemInfo.bytesPerCluster;
    size_t dirBlockSize = self.volume.systemInfo.dirBlockSize;
    size_t dirBlocksPerCluster = clusterSize / dirBlockSize;
    uint32_t startCluster = self.firstCluster;
    __block uint32_t numOfContigClusters = 0;
    __block uint32_t nextCluster = 0;
    uint32_t totalNumOfClusters = 0;

    NSError *error = nil;

    /*
     * The inner loop goes through the allocated clusters,
     * and saves up-to MAX_META_BLOCK_RANGES ranges in the array.
     * The outer loop calls metadataPurge: for these ranges.
     * We exit both loops once we reach the end of the cluster chain.
     */
    while ([self.volume.systemInfo isClusterValid:startCluster]) {
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

            [rangesToPurge addObject:[FSMetadataBlockRange rangeWithOffset:[self.volume.systemInfo offsetForCluster:startCluster]
                                                               blockLength:(uint32_t)dirBlockSize
                                                            numberOfBlocks:(uint32_t)(dirBlocksPerCluster * numOfContigClusters)]];

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

@end


