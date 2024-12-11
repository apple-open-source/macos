/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "FATManager.h"
#import "DirItem.h"

#define META_RW_BLOCK_LEN (3 * 4096U)

@implementation FATBlock

-(instancetype)init
{
    self = [super init];
    if (self) {
        _data = [[NSMutableData alloc] initWithLength:META_RW_BLOCK_LEN];
    }
    return self;
}

-(instancetype)initWithOffset:(uint64_t)offset
{
    self = [super init];
    if (self) {
        _data = [[NSMutableData alloc] initWithLength:META_RW_BLOCK_LEN];
        _startOffset = offset;
    }
    return self;
}

-(instancetype)initWithOffset:(uint64_t)offset
                    andLength:(uint64_t)length
{
    self = [super init];
    if (self) {
        _data = [[NSMutableData alloc] initWithLength:length];
        _startOffset = offset;
    }
    return self;
}

@end

@implementation FATManager

-(instancetype _Nullable)initWithDevice:(FSBlockDeviceResource *)device
                                   info:(FileSystemInfo *)info
                                    ops:(FSOperations *)fsOps
                             usingCache:(bool)usingCache;
{
    NSString *queueName = [NSString stringWithFormat:@"%@.%s", self.device.BSDName, "fatQueue"];

    self.useCache = usingCache;
    self.device = device;
    self.fsInfo = info;
    self.fsOps = fsOps;
    self.fatQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_CONCURRENT);
    if (self.fatQueue == NULL) {
        os_log_fault(fskit_std_log(), "Failed to create FAT queue");
        return nil;
    }

    if (self.fsInfo.fatSize > META_RW_BLOCK_LEN) {
        self.rwSize = META_RW_BLOCK_LEN;
    } else {
        self.rwSize = (uint32_t)self.fsInfo.fatSize;
    }

    if ([self updateFATStats] != nil) {
        return nil;
    }
    return self;
}

-(uint32_t)clustersPerBlock
{
    switch(self.fsInfo.type) {
        case FAT12:
            return self.rwSize * 2 / 3;
        case FAT16:
            return self.rwSize / 2;
        default:
            return self.rwSize / 4;
    }
}

/** For a given cluster, return the physical offset of the FAT entry that holds its entry */
-(uint64_t)getOffsetForClusterEntry:(uint32_t)cluster
{
    return self.fsInfo.fatOffset + [self.fsOps fatEntryOffsetForCluster:cluster];
}

/** For a given cluster, return the offset from which to start read a self.rwSize block which contains it. */
-(off_t)getRWOffsetForClusterEntry:(uint32_t)cluster
{
    /* Get the offset of the cluster inside the FAT */
    uint64_t offset = [self.fsOps fatEntryOffsetForCluster:cluster];
    /* Add the start of the FAT and return the start of the block which contains our offset */
    return self.fsInfo.fatOffset + (offset - offset % self.rwSize);
}

-(uint64_t)getOffsetForCluster:(uint32_t)cluster
                    inFatBlock:(FATBlock*)fatBlock
{
    return [self getOffsetForClusterEntry:cluster] - fatBlock.startOffset;
}

-(NSError * _Nullable)syncMetaReadFromFAT:(void *)buffer
                               startingAt:(off_t)volumeOffset
{
    NSError *error = nil;
    uint64_t length = 0;

    if (volumeOffset + self.rwSize > self.fsInfo.fatOffset + self.fsInfo.fatSize) {
        length = self.fsInfo.fatOffset + self.fsInfo.fatSize - volumeOffset;
    } else {
        length = self.rwSize;
    }

    error = [Utilities syncMetaReadFromDevice:self.device
                                         into:buffer
                                   startingAt:volumeOffset
                                       length:length];

    return error;
}

-(NSError * _Nullable)syncMetaWriteToFATs:(void *)buffer
                               startingAt:(off_t)volumeOffset
{
    NSError *error = nil;
    uint64_t length = 0;

    /*
     * When we have more than one FAT, the other ones are used for backup, so
     * they will contains the same information which is written to the 1st FAT,
     * but in a different offset - one (or more) <FAT size> bytes after.
     * Since we use a fixed rwSize, we need to make sure the write doesn't
     * exceed the FAT size and corrupts other areas on disk.
     * Here we first adjust the write length if needed, then add the <FAT size>
     * to the actual write offset according to the FAT number.
     */
    if (volumeOffset + self.rwSize > self.fsInfo.fatOffset + self.fsInfo.fatSize) {
        length = self.fsInfo.fatOffset + self.fsInfo.fatSize - volumeOffset;
    } else {
        length = self.rwSize;
    }

    for (int i = 0; i < self.fsInfo.numOfFATs; i++) {
        error = [Utilities metaWriteToDevice:self.device
                                        from:buffer
                                  startingAt:volumeOffset + i * self.fsInfo.fatSize
                                      length:length];
    }
    return error;
}

/** Set the number of free clusters, the first free cluster, and the dirty bit value. */
-(NSError *)updateFATStats
{
    uint32_t currCluster = 0;
    FATBlock *block = [[FATBlock alloc] initWithOffset:self.fsInfo.fatOffset andLength:self.rwSize];
    __block uint32_t firstFoundFreeCluster = 0;
    off_t readOffset = self.fsInfo.fatOffset;
    __block uint32_t countedFreeClusters = 0;
    __block bool first = true;
    __block NSError *error = nil;

    [self getDirtyBitValue:^(NSError * _Nullable dirtyBitError, dirtyBitValue value) {
        if (dirtyBitError) {
            error = dirtyBitError;
        } else {
            self.fsInfo.dirtyBitValue = value;
        }
    }];

    while (readOffset <= self.fsInfo.fatOffset + self.fsInfo.fatSize) {
        error = [self syncMetaReadFromFAT:[block.data mutableBytes] startingAt:readOffset];
        if (error) {
            goto out;
        }

        block.startOffset = readOffset;
        [self countFreeClustersInBlock:block
                            startingAt:currCluster ? currCluster : FIRST_VALID_CLUSTER
                          replyHandler:^(uint32_t foundClusters, uint32_t firstFreeCluster, bool isContig) {
            countedFreeClusters += foundClusters;
            if (first) {
                firstFoundFreeCluster = firstFreeCluster;
                first = false;
            }
        }];
        currCluster += [self clustersPerBlock];
        readOffset += self.rwSize;
    }

    if (self.fsInfo.freeClusters != 0 && self.fsInfo.freeClusters != countedFreeClusters) {
        os_log_debug(fskit_std_log(), "%s: counted free clusters number (%u) is not equal to the number read in boot (%llu) - updating", __func__, countedFreeClusters, self.fsInfo.freeClusters);
    }
    self.fsInfo.freeClusters = countedFreeClusters;

    if (self.fsInfo.firstFreeCluster != 0 && self.fsInfo.firstFreeCluster != firstFoundFreeCluster) {
        os_log_debug(fskit_std_log(), "%s: first free cluster found (%u) is not equal to the one read in boot (%u) - updating", __func__, firstFoundFreeCluster, self.fsInfo.firstFreeCluster);
    }
    self.fsInfo.firstFreeCluster = firstFoundFreeCluster;

out:
    return error;
}

-(void)countFreeClustersInBlock:(FATBlock *)block
                     startingAt:(uint32_t)startCluster
                   replyHandler:(nonnull void (^)(uint32_t foundClusters,
                                                  uint32_t firstFreeCluster,
                                                  bool isContig)) reply
{
    uint64_t offsetInBuffer = [self getOffsetForClusterEntry:startCluster] - block.startOffset;
    uint8_t *fatBlockData = (uint8_t*)([block.data mutableBytes]);
    uint32_t increment = [self.fsOps numBytesPerClusterInFat];
    uint64_t maxValidCluster = self.fsInfo.maxValidCluster;
    uint32_t currCluster = startCluster;
    uint32_t firstFreeCluster = 0;
    uint32_t foundClusters = 0;
    uint32_t entryValue = 0;
    uint8_t *entry = NULL;
    bool isContig = true;

    while (offsetInBuffer < self.rwSize && (currCluster <= maxValidCluster)) {
        entry = fatBlockData + offsetInBuffer;
        entryValue = [self.fsOps getNextClusterFromEntryForCluster:currCluster
                                                             entry:entry];
        if (entryValue == FREE_CLUSTER) {
            if (firstFreeCluster == 0) {
                firstFreeCluster = currCluster;
            }
            foundClusters++;
        } else {
            isContig = false;
        }
        currCluster++;
        if (increment) {
            /*
             * Profiling shows that calling getOffsetForClusterEntry: for very
             * large disks significantly hurts performance.
             * FAT12 stores 2 cluster numbers in 3 bytes, so we can't add a
             * constant number each iteration. As a result, a function call is
             * needed, but given that FAT12 can't be used for huge disks, it's ok.
             * FAT16 and FAT32 store a cluster number in 2/4 bytes, so we can
             * just add the size each iteration and avoid a function call.
             */
            offsetInBuffer += increment;
        } else {
            offsetInBuffer = [self getOffsetForClusterEntry:currCluster] - block.startOffset;
        }
    }

    reply(foundClusters, firstFreeCluster, isContig);
}

/**
 Given a block, searches it for a contiguous cluster chain of a given length.
 @param block The FAT block to search in
 @param length How many contiguous clusters are wanted
 @param mustBeContig Should the found clusters be contig to the start cluster or not
 @param startCluster Where to begin the search in the block. If 0, begin at the block's start
                    (0 is an illegal cluster value), else, this is the cluster's absolute number
                    in the FAT.
 @param reply Reply block to report back the startCluster of the sequence found (or 0 on failure)
                              and its length (or 0 on failure).
 */
-(void)findFreeClustersInBlock:(FATBlock *)block
                      ofLength:(uint32_t)length
                        contig:(bool)mustBeContig
                  startCluster:(uint32_t)startCluster
                  replyHandler:(void (^) (uint32_t startCluster,
                                          uint32_t length)) reply
{
    uint32_t firstCluster = startCluster;
    uint32_t currCluster = startCluster;
    uint32_t numContig = 0;
    uint8_t *entry = NULL;
    uint64_t offset = 0;
    uint32_t value = 0;

    if (startCluster == 0) {
        /* Need to start from the start of the block */
        firstCluster = currCluster = (uint32_t)(block.startOffset - self.fsInfo.fatOffset) * [self clustersPerBlock] / self.rwSize;
        if (firstCluster == 0) {
            /* We should always skip clusters 0 and 1 which are illegal */
            firstCluster = currCluster = 2;
        }
    } else {
        /* Make sure startCluster is in the given block */
        uint32_t startClusterOffset = (uint32_t)[self getOffsetForClusterEntry:startCluster];
        if (startClusterOffset < block.startOffset || startClusterOffset > block.startOffset + self.rwSize) {
            return reply(0, 0);
        }
    }

    offset = [self getOffsetForClusterEntry:currCluster] - block.startOffset;
    while ((offset < self.rwSize) && (currCluster <= self.fsInfo.maxValidCluster)) {
        entry = (uint8_t*)[block.data mutableBytes] + offset;
        value = [self.fsOps getNextClusterFromEntryForCluster:currCluster entry:entry];
        if (value == FREE_CLUSTER) {
            numContig++;
            currCluster++;
            if (numContig == length) {
                return reply (firstCluster, numContig);
            }
        } else if (!mustBeContig) {
            /* Need to start looking for a new chain */
            if (numContig) {
                return reply(firstCluster, numContig);
            }
            firstCluster = ++currCluster;
            numContig = 0;
        } else {
            /* Can't start a new chain, must start it from startCluster */
            return reply(0, 0);
        }
        offset = [self getOffsetForClusterEntry:currCluster] - block.startOffset;
    }

    return reply(firstCluster, numContig);
}

/**
 Set up to numClusters as in use starting from startCluster.
 */
-(void)allocateClustersInBlock:(void*)block
                   numClusters:(uint32_t)numClusters
             startingAtCluster:(uint32_t)startCluster
                   startOffset:(uint32_t)startOffset
                  mustBeContig:(bool)mustBeContig
                  replyHandler:(void (^)(NSError *_Nullable error,
                                           uint32_t clustersAllocated,
                                           uint32_t lastAllocatedCluster,
                                           uint8_t* lastAllocatedEntry)) reply
{
    uint32_t currCluster = startCluster;
    uint32_t lastAllocatedCluster = 0;
    uint32_t currOffsetInBuffer = 0;
    uint32_t nextOffsetInBuffer = 0;
    uint32_t allocatedClusters = 0;
    uint32_t currEntryValue = 0;
    uint32_t nextEntryValue = 0;
    uint32_t nextCluster = currCluster + 1;
    uint8_t *currEntry = NULL;
    uint8_t *nextEntry = NULL;

    off_t blockOffset = [self getRWOffsetForClusterEntry:currCluster];
    currOffsetInBuffer = (uint32_t)[self getOffsetForClusterEntry:currCluster] - startOffset;
    currEntry = (uint8_t*)(block) + currOffsetInBuffer;

    /*
     * Iterate over the block using nested loops:
     * The external one searches for the cluster to be allocated, the inner one
     * searches for the next one. For example: in the external loop we found
     * that cluster 23 is free, we need to make sure, in the inner loop, that
     * cluster 24 is also free so we can set cluster 23 to point to 24 (or
     * another one, if 24 is in use).
     * For this reason the external loop runs up to but not including the last
     * entry of a block.
     */
    while ((allocatedClusters < numClusters) &&
           (currCluster <= self.fsInfo.maxValidCluster) &&
           ([self getRWOffsetForClusterEntry:currCluster] == blockOffset) &&
           (nextOffsetInBuffer < self.rwSize)) {
        currEntryValue = [self.fsOps getNextClusterFromEntryForCluster:currCluster
                                                                 entry:currEntry];
        if (currEntryValue == FREE_CLUSTER) {
            nextCluster = currCluster + 1;
            nextOffsetInBuffer = (uint32_t)[self getOffsetForClusterEntry:nextCluster] - startOffset;
            allocatedClusters++;
            lastAllocatedCluster = currCluster;

            /* First check EOF */
            if (allocatedClusters == numClusters) {
                [self.fsOps setFatEntryForCluster:currCluster
                                            entry:currEntry
                                        withValue:EOF_CLUSTER & self.fsInfo.FATMask];
                break;
            }

            bool foundNext = false;
            while ((nextOffsetInBuffer < self.rwSize) && (nextCluster <= self.fsInfo.maxValidCluster)) {
                nextEntry = (uint8_t*)(block) + nextOffsetInBuffer;
                nextEntryValue = [self.fsOps getNextClusterFromEntryForCluster:nextCluster
                                                                         entry:nextEntry];
                if (nextEntryValue == FREE_CLUSTER) {
                    [self.fsOps setFatEntryForCluster:currCluster
                                                entry:currEntry
                                            withValue:nextCluster & self.fsInfo.FATMask];
                    currCluster = nextCluster;
                    currOffsetInBuffer = nextOffsetInBuffer;
                    currEntry = (uint8_t*)(block) + currOffsetInBuffer;
                    foundNext = true;
                    break;
                } else {
                    nextCluster++;
                    nextOffsetInBuffer = (uint32_t)[self getOffsetForClusterEntry:nextCluster] - startOffset;
                }
            }

            if (!foundNext) {
                // We did not link the last allocated cluster to a next cluster in the same block.
                // For now mark it as EOF and let the caller link it or free it later
                [self.fsOps setFatEntryForCluster:currCluster
                                            entry:currEntry
                                        withValue:EOF_CLUSTER & self.fsInfo.FATMask];
                break;
            }

        } else {
            /*
             * We either got currCluster as a valid start point or just found
             * it to be free in the loop above.
             */
            os_log_fault(fskit_std_log(), "%s: Cluster %u is not free", __FUNCTION__, currCluster);
            return reply(fs_errorForPOSIXError(EINVAL), 0, 0, 0);
        }
    }

    /* Edge case: We start iterating from the last cluster in a given block */
    if ((allocatedClusters == 0) && (currCluster == startCluster) && ([self getRWOffsetForClusterEntry:currCluster] + self.rwSize == [self getRWOffsetForClusterEntry:nextCluster])) {
        lastAllocatedCluster = currCluster;
    }

    return reply(nil, allocatedClusters, lastAllocatedCluster, currEntry);
}

/**
 Main allocation function.
 Allocate clusters starting at the beginning of the FAT / a specific cluster.
 @param numOfClusters How many clusters to allocate
 @param searchFromCluster a cluster to start the search for available clusters from. It is not guaranteed that the
                         first allocated cluster will be searchFromCluster. If searchFromCluster is zero, the search
                         will start from the next available cluster in the FAT.
 @param allowPartial If false, failing to fully allocate clusters will return an error
 @param zeroFill CURRENTLY IGNORED
 @param mustBeContig If true, only allocate a single cluster chain.
 @param reply In case of a failure returns a non-nil error and zeros. Else, error is nil and the first, last and count of clusters allocated is returned.
 */
-(void)allocateClusters:(uint32_t)numOfClusters
      searchFromCluster:(uint32_t)searchFromCluster
           allowPartial:(bool)allowPartial
               zeroFill:(bool)zeroFill
           mustBeContig:(bool)mustBeContig
           replyHandler:(void (^)(NSError * _Nullable error,
                                  uint32_t firstAllocatedCluster,
                                  uint32_t lastAllocatedCluster,
                                  uint32_t numAllocated)) reply
{
    dispatch_barrier_sync(self.fatQueue, ^{
        __block FATBlock *currFatBlock = [[FATBlock alloc] initWithOffset:0 andLength:self.rwSize];
        FATBlock *nextFatBlock = [[FATBlock alloc] initWithOffset:0 andLength:self.rwSize];
        __block uint32_t firstAllocatedCluster = 0;
        __block uint32_t lastAllocatedCluster = 0;
        __block uint32_t allocatedClusters = 0;
        __block uint32_t currCluster = 0;
        __block uint32_t nextCluster = 0;
        __block NSError *nsError = nil;
        uint32_t nextEntryValue = 0;
        uint8_t *currEntry = NULL;
        uint8_t *nextEntry = NULL;
        off_t currReadOffset = 0;
        off_t nextReadOffset = 0;
        uint32_t clustersToAlloc = numOfClusters;
        /* First see if we can fill the request at all */
        if (clustersToAlloc > self.fsInfo.freeClusters) {
            if (allowPartial && self.fsInfo.freeClusters) {
                os_log_debug(fskit_std_log(), "%s: (allowPartial = true) %u clusters requested,"\
                             "but only %llu are available. Will try to allocate %llu clusters.",\
                             __func__, clustersToAlloc, self.fsInfo.freeClusters, self.fsInfo.freeClusters);
                clustersToAlloc = (uint32_t)self.fsInfo.freeClusters;
            } else {
                os_log_error(fskit_std_log(), "%s: (allowPartial = %d) %u clusters requested,"\
                             "but only %llu are available. Returning ENOSPC.",\
                             __func__, allowPartial, clustersToAlloc, self.fsInfo.freeClusters);
                return reply(fs_errorForPOSIXError(ENOSPC), 0, 0, 0);
            }
        }

        if (clustersToAlloc == 0) {
            return reply(nil, 0, 0, 0);
        }

        if (searchFromCluster) {
            nextCluster = searchFromCluster;
        } else if (self.fsInfo.firstFreeCluster < self.fsInfo.maxValidCluster){
            nextCluster = self.fsInfo.firstFreeCluster;
        }
        
         do {
             [self findNextFreeCluster:nextCluster
                          replyHandler:^(NSError * _Nullable error, uint32_t cluster, uint32_t contigLength) {
                 nsError = error;
                 nextCluster = cluster;
             }];

             if (nsError) {
                 goto out;
             }

             nextReadOffset = [self getRWOffsetForClusterEntry:nextCluster];
             nextFatBlock.startOffset = nextReadOffset;
             if (nextFatBlock.data != nil) {
                 nextEntry = (uint8_t *)[nextFatBlock.data mutableBytes];
             }

             nsError = [self syncMetaReadFromFAT:[nextFatBlock.data mutableBytes] startingAt:nextReadOffset];

             currReadOffset = [self getRWOffsetForClusterEntry:currCluster];
             if (currCluster) {
                 if (currReadOffset != nextReadOffset) {
                     /*
                      * We need to connect the newly allocated clusters to the existing
                      * cluster chain, and it's in a new FAT block.
                      */
                     currFatBlock.startOffset = currReadOffset;
                     nsError = [self syncMetaReadFromFAT:[currFatBlock.data mutableBytes] startingAt:currReadOffset];
                     if (nsError) {
                         goto out;
                     }
                 } else {
                     currFatBlock = nextFatBlock;
                 }

                 currEntry = (uint8_t *) [currFatBlock.data mutableBytes] + ([self getOffsetForClusterEntry:currCluster] - currReadOffset);
                 nextEntry = (uint8_t *) [nextFatBlock.data mutableBytes] + ([self getOffsetForClusterEntry:nextCluster] - nextReadOffset);

                 nextEntryValue = [self.fsOps getNextClusterFromEntryForCluster:nextCluster
                                                                          entry:nextEntry];
                 /* We expect the new cluster to be free */
                 if (nextEntryValue != FREE_CLUSTER) {
                     os_log_error(fskit_std_log(), "%s: Cluster (%u) isn't free, curr cluster %u", __FUNCTION__, nextCluster, currCluster);
                     nsError = fs_errorForPOSIXError(EINVAL);
                     goto out;
                 }

                 /* Connect the previous cluster to the new */
                 [self.fsOps setFatEntryForCluster:currCluster
                                             entry:currEntry
                                         withValue:nextCluster & self.fsInfo.FATMask];
                 nsError = [self syncMetaWriteToFATs:[currFatBlock.data mutableBytes]
                                          startingAt:currReadOffset];
                 if (nsError != nil) {
                     os_log_fault(fskit_std_log(), "%s: Failed to write to the device", __FUNCTION__);
                     nsError = fs_errorForPOSIXError(EIO);
                     goto out;
                 }
            }

            [self allocateClustersInBlock:[nextFatBlock.data mutableBytes]
                              numClusters:clustersToAlloc - allocatedClusters
                        startingAtCluster:nextCluster
                              startOffset:(uint32_t)nextReadOffset
                             mustBeContig:mustBeContig
                             replyHandler:^(NSError * _Nullable error, uint32_t numAllocated, uint32_t lastCluster, uint8_t *lastAllocatedEntry) {
                if (error) {
                    return reply(error, 0 ,0, 0);
                }

                if (numAllocated == 0) {
                    os_log_fault(fskit_std_log(), "%s: allocateClustersInBlock could not allocate any cluster", __FUNCTION__);
                    return reply(fs_errorForPOSIXError(EIO), 0 ,0, 0);
                } else {
                    if (firstAllocatedCluster == 0) {
                        firstAllocatedCluster = nextCluster;
                    }
                    allocatedClusters += numAllocated;
                    lastAllocatedCluster = lastCluster;
                    self.fsInfo.freeClusters -= numAllocated;
                    /* Write back the block we modified */
                    [self syncMetaWriteToFATs:[nextFatBlock.data mutableBytes]
                                   startingAt:nextReadOffset];
                }
            }];

             /*
              * We need to break after the reply block if we are done, else
              * we'll be passing wrong input to findFreeClustersInBlock
              */
             currCluster = lastAllocatedCluster;
             nextCluster = lastAllocatedCluster + 1;

             if (nextCluster > self.fsInfo.maxValidCluster) {
                 nextCluster = FIRST_VALID_CLUSTER;
             }
         } while (allocatedClusters < clustersToAlloc);

        if ((allocatedClusters < clustersToAlloc) && !allowPartial) {
            /* This should have been caught at the sanity checks - fault, maybe more? */
            os_log_fault(fskit_std_log(), "%s: Allocated %u/%u clusters, filesystems free clusters %llu",
                         __FUNCTION__, allocatedClusters, clustersToAlloc, self.fsInfo.freeClusters);
        }
out:
        if (!nsError) {
            [self findNextFreeCluster:nextCluster
                         replyHandler:^(NSError * _Nullable error, uint32_t cluster, uint32_t contigLength) {
                if (contigLength) {
                    self.fsInfo.firstFreeCluster = cluster;
                } else {
                    self.fsInfo.firstFreeCluster = 0;
                }
            }];
        }

        /* Update the changes we made */
        return reply(nsError,
                     nsError ? 0 : firstAllocatedCluster,
                     nsError ? 0 : lastAllocatedCluster,
                     nsError ? 0 : allocatedClusters);
    });

}

-(void)allocateClusters:(uint32_t)numOfClusters
           allowPartial:(bool)allowPartial
               zeroFill:(bool)zeroFill
           mustBeContig:(bool)mustBeContig
           replyHandler:(void (^)(NSError * _Nullable error,
                                  uint32_t firstAllocatedCluster,
                                  uint32_t lastAllocatedCluster,
                                  uint32_t numAllocated)) reply
{
    if (numOfClusters == 0) {
        return reply(nil, 0, 0, 0);
    }

    [self allocateClusters:numOfClusters
         searchFromCluster:0
              allowPartial:allowPartial
                  zeroFill:zeroFill
              mustBeContig:mustBeContig
              replyHandler:^(NSError * _Nullable error, uint32_t firstAllocatedCluster, uint32_t lastAllocatedCluster, uint32_t allocatedClusters) {
        return reply(error, firstAllocatedCluster, lastAllocatedCluster, allocatedClusters);
    }];
}

-(void)allocateClusters:(uint32_t)numOfClusters
                forItem:(FATItem *)theItem
           allowPartial:(bool)allowPartial
           mustBeContig:(bool)mustBeContig
               zeroFill:(bool)zeroFill
           replyHandler:(void (^)(NSError * _Nullable error,
                                  uint32_t firstAllocatedCluster,
                                  uint32_t lastAllocatedCluster,
                                  uint32_t numAllocated)) reply
{
    NSMutableData *prevFatBlock = [[NSMutableData alloc] initWithLength:self.rwSize]; /* Read buffer for the item's last cluster */

    uint32_t clusterToSearchFrom = 0;
    __block NSError *nsError = nil;

    if (numOfClusters == 0) {
        return reply(nil, 0, 0, 0);
    }

    /*
     * If We're connecting an exiting cluster chain, we should do the best we can to find
     * more available clusters as close as we can in the FAT.
     */
    clusterToSearchFrom = ((theItem.numberOfClusters == 0) ||
                           (theItem.lastCluster == self.fsInfo.maxValidCluster)) ? 0 : theItem.lastCluster + 1;
    [self allocateClusters:numOfClusters
         searchFromCluster:clusterToSearchFrom
              allowPartial:allowPartial
                  zeroFill:zeroFill
              mustBeContig:mustBeContig
              replyHandler:^(NSError * _Nullable error, uint32_t firstAllocatedCluster, uint32_t lastAllocatedCluster, uint32_t allocatedClusters) {

        if (allocatedClusters) {
            if (theItem.numberOfClusters == 0) {
                theItem.firstCluster = firstAllocatedCluster;
            } else {
                /* If the item has previously allocated clusters, they should be linked */
                off_t prevReadOffset = [self getRWOffsetForClusterEntry:theItem.lastCluster];
                nsError = [self syncMetaReadFromFAT:[prevFatBlock mutableBytes] startingAt:prevReadOffset];
                if (nsError) {
                    return reply(nsError, 0, 0, 0);
                }

                uint8_t *lastClusterEntry;
                lastClusterEntry = (uint8_t *)[prevFatBlock mutableBytes] + [self getOffsetForClusterEntry:theItem.lastCluster] - prevReadOffset;

                [self.fsOps setFatEntryForCluster:theItem.lastCluster
                                            entry:lastClusterEntry
                                        withValue:firstAllocatedCluster & self.fsInfo.FATMask];

                /* Write back the linked cluster */
                nsError = [self syncMetaWriteToFATs:[prevFatBlock mutableBytes]
                                         startingAt:prevReadOffset];

                if (nsError) {
                    return reply(nsError, 0, 0, 0);
                }
            }

            theItem.lastCluster = lastAllocatedCluster;
            theItem.firstClusterIndexInLastAllocation = theItem.numberOfClusters;
            theItem.firstClusterInLastAllocation = firstAllocatedCluster;
            theItem.numberOfClusters += allocatedClusters;
        }
        reply(error, firstAllocatedCluster, lastAllocatedCluster, allocatedClusters);
    }];
}

/** Free the last numClusters clusters of theItem  */
-(void)freeClusters:(uint32_t)numClusters
             ofItem:(FATItem *)theItem
       replyHandler:(void (^) (NSError * _Nullable error)) reply
{
    dispatch_barrier_sync(self.fatQueue, ^{
        NSMutableData *block = [[NSMutableData alloc] initWithLength:self.rwSize];
        uint32_t currCluster = theItem.firstCluster;
        uint32_t clustersCounter = 0;
        uint32_t freedClusters = 0;
        uint32_t newEOFCluster = 0;
        uint32_t entryValue = 0;
        NSError *nsError = nil;
        uint8_t *entry = NULL;
        off_t entryOffset = 0;
        off_t readOffset = 0;

        /* sanity checks */
        if (numClusters == 0) {
            os_log_fault(fskit_std_log(), "%s: Received 0 clusters to free", __FUNCTION__);
            return reply(fs_errorForPOSIXError(EINVAL));
        }

        if (numClusters > theItem.numberOfClusters || currCluster < FIRST_VALID_CLUSTER || currCluster > self.fsInfo.maxValidCluster) {
            return reply(fs_errorForPOSIXError(EINVAL));
        }

        if (block == NULL) {
            return reply(fs_errorForPOSIXError(ENOMEM));
        }

        /*
         * Partial release of the last numClusters of the item.
         * Start from the item's first cluster, find the item's new EOF and
         * the cluster from which to start
         */
        while (clustersCounter < theItem.numberOfClusters - numClusters) {
            readOffset = [self getRWOffsetForClusterEntry:currCluster];
            nsError = [self syncMetaReadFromFAT:[block mutableBytes] startingAt:readOffset];
            if (nsError) {
                return reply(nsError);
            }

            entryOffset = [self getOffsetForClusterEntry:currCluster];

            while (entryOffset - readOffset < self.rwSize && entryOffset - readOffset >= 0) {
                bool done = false;
                entry = (uint8_t*)[block mutableBytes] + (entryOffset - readOffset);
                entryValue = [self.fsOps getNextClusterFromEntryForCluster:currCluster entry:entry];
                if (clustersCounter == theItem.numberOfClusters - numClusters - 1) {
                    /* Found our new EOF */
                    newEOFCluster = currCluster;
                    [self.fsOps setFatEntryForCluster:currCluster
                                                entry:entry
                                            withValue:EOF_CLUSTER & self.fsInfo.FATMask];
                    done = true;
                }
                clustersCounter++;

                currCluster = entryValue;
                entryOffset = [self getOffsetForClusterEntry:currCluster];
                if (done) {
                    break;
                }
            }
        }

        /*
         * If we are to free all the item's clusters, set the starting point
         * of releasing to the item's first cluster.
         */
        if (numClusters == theItem.numberOfClusters) {
            currCluster = theItem.firstCluster;
            entryOffset = [self getOffsetForClusterEntry:currCluster];
            readOffset = [self getRWOffsetForClusterEntry:currCluster];
            nsError = [self syncMetaReadFromFAT:[block mutableBytes] startingAt:readOffset];
            if (nsError) {
                return reply(nsError);
            }
        }

        if ((readOffset != [self getRWOffsetForClusterEntry:currCluster]) && (clustersCounter == theItem.numberOfClusters - numClusters - 1)) {
            /*
             * We can get here in a scenario of a partial free, e.g. we have
             * clusters [11, 20] and [1000, 1010] and we need to free 10 clusters.
             * At this point we set cluster 20 as the new EOF and currCluster is
             * #1000. We went to resetBlock label but didn't read a new block
             * as we just reached 10 clusters.
             */
            readOffset = [self getRWOffsetForClusterEntry:currCluster];
            entryOffset = [self getOffsetForClusterEntry:currCluster];
            nsError = [self syncMetaReadFromFAT:[block mutableBytes] startingAt:readOffset];
            if (nsError) {
                return reply(nsError);
            }
        }

        /*
         * At this point currCluster points to the first cluster to be freed.
         * Iterate up to the last cluster, free everything on the way.
         */
        while (freedClusters < numClusters && (currCluster <= self.fsInfo.maxValidCluster)) {
            while ((entryOffset >= readOffset) && ((entryOffset - readOffset) < self.rwSize) && (freedClusters < numClusters) && (currCluster <= self.fsInfo.maxValidCluster)) {
                entry = (uint8_t*)[block mutableBytes] + (entryOffset - readOffset);

                /* Save the next cluster to be freed */
                entryValue = [self.fsOps getNextClusterFromEntryForCluster:currCluster
                                                                     entry:entry];
                if (entryValue == FREE_CLUSTER) {
                    /* This is unexpected. Corruption? */
                    os_log_error(fskit_std_log(), "%s: cluster %u is free where it should be in use. Item stats [%u, %u, %u]",
                                 __FUNCTION__, currCluster, theItem.firstCluster, theItem.lastCluster, theItem.numberOfClusters);
                    nsError = fs_errorForPOSIXError(EIO);
                    break;
                }
                bool isEof = [self isEOFCluster:entryValue];

                [self.fsOps setFatEntryForCluster:currCluster
                                            entry:entry
                                        withValue:FREE_CLUSTER & self.fsInfo.FATMask];
                self.fsInfo.freeClusters++;
                freedClusters++;

                if ((self.fsInfo.firstFreeCluster > currCluster) || (self.fsInfo.firstFreeCluster == 0)) {
                    self.fsInfo.firstFreeCluster = currCluster;
                }

                /* Sanity: make sure we've reached the desired number of free clusters */
                if (isEof && (freedClusters != numClusters)) {
                    os_log_error(fskit_std_log(), "%s: %u freed clusters %u, should have freed %u, got EOF", __FUNCTION__,currCluster, freedClusters, numClusters);
                    nsError = fs_errorForPOSIXError(EIO);
                    break;
                }

                currCluster = entryValue;
                entryOffset = [self getOffsetForClusterEntry:currCluster];
            }
            /* Write to the device what we just freed, don't override errors from inner loop */
            nsError = [self syncMetaWriteToFATs:[block mutableBytes]
                                     startingAt:readOffset];
            if (nsError) {
                return reply(nsError);
            }
            if (freedClusters < numClusters) {
                readOffset = [self getRWOffsetForClusterEntry:currCluster];
                nsError = [self syncMetaReadFromFAT:[block mutableBytes] startingAt:readOffset];
                if (nsError) {
                    return reply(nsError);
                }
            }
        }
        if (currCluster > self.fsInfo.maxValidCluster && ![self isEOFCluster:currCluster]) {
            os_log_fault(fskit_std_log(), "%s: curr cluster is illegal (%u)", __FUNCTION__, currCluster);
        }

        /* After writing to disk, update the item */
        if (newEOFCluster) {
            theItem.lastCluster = newEOFCluster;
        }
        if (numClusters == theItem.numberOfClusters) {
            theItem.firstCluster = theItem.lastCluster = 0;
        }
        theItem.firstClusterInLastAllocation = 0;
        theItem.firstClusterIndexInLastAllocation = 0;
        theItem.numberOfClusters -= freedClusters;
        return reply(nil);
    });
}

-(void)freeClusterFrom:(uint32_t)startCluster
           numClusters:(uint32_t)numClusters
          replyHandler:(void(^)(NSError * _Nullable error))reply
{
    dispatch_barrier_sync(self.fatQueue, ^{
        FATBlock *fatBlock = [[FATBlock alloc] initWithOffset:0 andLength:self.rwSize];
        uint32_t currCluster = startCluster;
        uint32_t freedClusters = 0;
        uint32_t entryValue = 0;
        off_t entryOffset = 0;
        uint8_t* entry = NULL;
        NSError *error = nil;
        off_t readOffset = 0;

        while ((![self isEOFCluster:entryValue]) && (freedClusters < numClusters)) {
            readOffset = [self getRWOffsetForClusterEntry:currCluster];
            error = [self syncMetaReadFromFAT:[fatBlock.data mutableBytes] startingAt:readOffset];
            if (error) {
                return reply(error);
            }

            entryOffset = [self getOffsetForClusterEntry:currCluster];

            while ((entryOffset - readOffset < self.rwSize) && (![self isEOFCluster:entryValue]) && (freedClusters < numClusters)) {
                /* Free the clusters in the current block */
                entry = (uint8_t*)[fatBlock.data mutableBytes] + (entryOffset - readOffset);
                entryValue = [self.fsOps getNextClusterFromEntryForCluster:currCluster entry:entry];
                /* Free the current cluster once we saved its next */
                [self.fsOps setFatEntryForCluster:currCluster
                                            entry:entry
                                        withValue:FREE_CLUSTER & self.fsInfo.FATMask];
                freedClusters++;
                self.fsInfo.freeClusters++;
                if ((self.fsInfo.firstFreeCluster > currCluster) || (self.fsInfo.firstFreeCluster == 0)) {
                    self.fsInfo.firstFreeCluster = currCluster;
                }
                currCluster = entryValue;
                entryOffset = [self getOffsetForClusterEntry:currCluster];
            }

            [self syncMetaWriteToFATs:[fatBlock.data mutableBytes]
                           startingAt:readOffset];
        }

        if (freedClusters == numClusters) {
            return reply(nil);
        } else {
            /* We freed the wrong amount of clusters for some reason */
            return reply(fs_errorForPOSIXError(EINVAL));
        }
    });
}

-(void)fatIterator:(uint32_t)startCluster
      replyHandler:(iterateClustersResult (^)(NSError * _Nullable error,
                                              uint32_t startCluster,
                                              uint32_t numOfContigClusters,
                                              uint32_t nextCluster))reply
{
    __block FATBlock *fatBlock = [[FATBlock alloc] initWithOffset:0 andLength:self.rwSize];
    uint32_t startOfCurrentSequence = startCluster;
    uint32_t cluster = startCluster;
    uint32_t contigClusters = 1;
    uint64_t clusterOffset = 0;
    uint32_t entryValue = 0;
    uint8_t* entry = NULL;
    NSError *error = nil;
    off_t readOffset = 0;

    while (true) {
        readOffset = [self getRWOffsetForClusterEntry:cluster];
        fatBlock.startOffset = [self getRWOffsetForClusterEntry:cluster];
        error = [self syncMetaReadFromFAT:[fatBlock.data mutableBytes] startingAt:readOffset];
        if (error) {
            reply(error, startOfCurrentSequence, contigClusters, 0);
            return;
        }

        clusterOffset = [self getOffsetForCluster:cluster inFatBlock:fatBlock];
        while (clusterOffset < self.rwSize) {
            entry = (uint8_t*)([fatBlock.data mutableBytes]) + clusterOffset;
            entryValue = [self.fsOps getNextClusterFromEntryForCluster:cluster entry:entry];
            if (!entryValue) {
                os_log_error(fskit_std_log(), "%s: Unexpected free cluster %u", __FUNCTION__, cluster);
                reply(fs_errorForPOSIXError(EFAULT), 0, 0, 0);
                return;
            }
            if ([self isEOFCluster:entryValue]) {
                /* Nothing else to do */
                reply(nil, startOfCurrentSequence, contigClusters, entryValue);
                return;
            }
            /* Getting here means entryValue is the next cluster in chain */
            /* If contig, go ahead, else reply */
            if (cluster + 1 == entryValue) {
                contigClusters++;
                cluster = entryValue;
            } else {
                if (reply(nil, startOfCurrentSequence, contigClusters, entryValue) == iterateClustersContinue) {
                    startOfCurrentSequence = entryValue;
                    cluster = entryValue;
                    contigClusters = 1;
                } else {
                    return;
                }
            }
            clusterOffset = [self getOffsetForCluster:cluster inFatBlock:fatBlock];
        }
    }
}

/**
 Iterate over an item's contiguous cluster chain starting at the given cluster.
 reply its length as well as the start cluster of the next cluster chain (or EOF).
 */
-(void)getContigClusterChainLengthStartingAt:(uint32_t)startCluster
                                replyHandler:(void (^)(NSError * _Nullable error,
                                                       uint32_t numOfContigClusters,
                                                       uint32_t nextCluster))reply
{
    dispatch_block_t block = ^{
        [self fatIterator:startCluster replyHandler:^iterateClustersResult(NSError * _Nullable error,
                                                                    uint32_t startCluster,
                                                                    uint32_t numOfContigClusters,
                                                                    uint32_t nextCluster) {
            if (error) {
                reply(error, 0, 0);
                return iterateClustersStop;
            }
            reply(nil, numOfContigClusters, nextCluster);
            return iterateClustersStop;
        }];
    };
    
    if (strcmp(dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL), dispatch_queue_get_label(self.fatQueue)) == 0) {
        block();
    } else {
        dispatch_sync(self.fatQueue, block);
        
    }
}

/**
 Get the number of clusters allocated to the given item.
 The reply block returns both the cluster chain length and the last cluster, as
 clusters are not always contiguous.
 */
-(void)clusterChainLength:(FATItem*)item
             replyHandler:(void (^)(NSError * _Nullable error,
                                    uint32_t lastCluster,
                                    uint32_t length))reply
{
    dispatch_block_t block = ^{
        __block uint32_t clusterCount = 0;
        __block uint32_t lastCluster = 0;
        
        /* Check if we got a directory. If so, check if it's FAT12/16 root dir. If so, exit.
         (the FAT12/16 root dir data is stored at a known offset, separately from the other files/dirs data). */
        DirItem *dirItem = nil;
        if ((dirItem = [DirItem dynamicCast:item]) && [dirItem isFat1216RootDir]) {
            reply(nil, 0, 0);
            return;
        }


        if (item.firstCluster == 0) {
            reply(fs_errorForPOSIXError(EINVAL), 0, 0);
            return;
        }

        [self fatIterator:item.firstCluster
             replyHandler:^iterateClustersResult(NSError * _Nullable error,
                                                 uint32_t startCluster,
                                                 uint32_t numOfContigClusters,
                                                 uint32_t nextCluster) {
            if (error) {
                reply(error, startCluster + numOfContigClusters - 1, numOfContigClusters);
                return iterateClustersStop;
            }
            
            clusterCount += numOfContigClusters;
            lastCluster = startCluster + numOfContigClusters - 1;
            return iterateClustersContinue;
        }];
        reply(nil, lastCluster, clusterCount);
    };
        
    if (strcmp(dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL), dispatch_queue_get_label(self.fatQueue)) == 0) {
        block();
    } else {
        dispatch_sync(self.fatQueue, block);
        
    }
}

/**
 Iterate over the given item's cluster chain. When a contiguous sequence ends
 (meaning the current cluster +1 != next cluster), reply with the current sequence.
 If the response to the reply is iterateClustersContinue, move on to the next
 sequence. Else, stop and return.
 */
-(void)iterateClusterChainOfItem:(FATItem *)item
                    replyHandler:(iterateClustersResult (^)(NSError * _Nullable error,
                                                            uint32_t startCluster,
                                                            uint32_t numOfContigClusters))reply
{
    dispatch_sync(self.fatQueue, ^{

        /* Check if we got a directory. If so, check if it's FAT12/16 root dir. If so, exit.
         (the FAT12/16 root dir data is stored at a known offset, separately from the other files/dirs data). */
        DirItem *dirItem = nil;
        if ((dirItem = [DirItem dynamicCast:item]) && [dirItem isFat1216RootDir]) {
            reply(nil, 0, 0);
            return;
        }

        if (item.firstCluster == 0) {
            reply(fs_errorForPOSIXError(EINVAL), 0, 0);
            return;
        }

        [self fatIterator:item.firstCluster
             replyHandler:^iterateClustersResult(NSError * _Nullable innerError,
                                                 uint32_t startCluster,
                                                 uint32_t numOfContigClusters,
                                                 uint32_t nextCluster) {
            if (innerError) {
                return reply(innerError, startCluster, numOfContigClusters);
            }
            return reply(nil, startCluster, numOfContigClusters);
        }];
    });
}

/** Fetch and return the dirty bit value from the FAT. */
-(void)getDirtyBitValue:(void (^)(NSError * _Nullable error,
                                  dirtyBitValue value))reply
{
    dispatch_sync(self.fatQueue, ^{
        FATBlock *fatBlock = [[FATBlock alloc] initWithOffset:[self getRWOffsetForClusterEntry:[self.fsOps getDirtyBitCluster]] andLength:self.rwSize];

        NSError *error = [self syncMetaReadFromFAT:fatBlock.data.mutableBytes startingAt:fatBlock.startOffset];
        if (error) {
            os_log_error(fskit_std_log(), "%s: Couldn't read FAT block from disk. Error = %@.", __FUNCTION__, error);
            return reply(error, dirtyBitUnknown);
        }

        uint8_t *dirtyBitEntry = (uint8_t*)(fatBlock.data.mutableBytes) + [self getOffsetForCluster:[self.fsOps getDirtyBitCluster] inFatBlock:fatBlock];

        return reply(nil, [self.fsOps getDirtyBitValueForEntry:dirtyBitEntry]);
    });
}

/** Set the given value to the dirty bit location in FAT. */
-(void)setDirtyBitValue:(dirtyBitValue)newValue
           replyHandler:(void (^)(NSError * _Nullable error))reply
{
    dispatch_barrier_sync(self.fatQueue, ^{

        NSError *error = nil;

        if (self.fsInfo.type == FAT12) {
            /* In FAT12 we don't have a dirty bit. */
            return reply(nil);
        }

        if (self.fsInfo.dirtyBitValue != newValue) {
            /* Write to FAT only when we're really changing the dirty bit value. */

            FATBlock *fatBlock = [[FATBlock alloc] initWithOffset:[self getRWOffsetForClusterEntry:[self.fsOps getDirtyBitCluster]] andLength:self.rwSize];

            error = [self syncMetaReadFromFAT:fatBlock.data.mutableBytes startingAt:fatBlock.startOffset];
            if (error) {
                os_log_error(fskit_std_log(), "%s: Couldn't read FAT block from disk. Error = %@.", __FUNCTION__, error);
                return reply(error);
            }

            uint8_t *dirtyBitEntry = (uint8_t*)(fatBlock.data.mutableBytes) + [self getOffsetForCluster:[self.fsOps getDirtyBitCluster] inFatBlock:fatBlock];

            [self.fsOps applyDirtyBitValueToEntry:dirtyBitEntry
                                         newValue:newValue];

            error = [self syncMetaWriteToFATs:fatBlock.data.mutableBytes
                                   startingAt:fatBlock.startOffset];
            if (error) {
                os_log_error(fskit_std_log(), "%s: Couldn't write FAT block to disk. Error = %@.", __FUNCTION__, error);
                return reply(error);
            }

            self.fsInfo.dirtyBitValue = newValue;
        }
        return reply(nil);
    });
}

-(bool)isEOFCluster:(uint64_t)cluster
{
    if (cluster >= (EOF_RANGE_START & self.fsInfo.FATMask) && cluster <= (EOF_RANGE_END & self.fsInfo.FATMask)) {
        return YES;
    }
    return NO;
}

-(void)findNextFreeCluster:(uint32_t)startFromCluster
              replyHandler:(void (^)(NSError * _Nullable error,
                                     uint32_t cluster,
                                     uint32_t contigLength))reply
{
    __block uint32_t nextChainLength = 0;
    __block uint32_t currCluster = startFromCluster;
    FATBlock *fatBlock = [[FATBlock alloc] initWithOffset:0 andLength:self.rwSize];
    off_t startBlockOffset = [self getRWOffsetForClusterEntry:currCluster];
    bool wrapAround = false;

    while (true) {
        fatBlock.startOffset = [self getRWOffsetForClusterEntry:currCluster];

        NSError *error = [self syncMetaReadFromFAT:[fatBlock.data mutableBytes] startingAt:fatBlock.startOffset];
        if (error) {
            return reply(error, 0, 0);
        }

        [self findFreeClustersInBlock:fatBlock
                             ofLength:1
                               contig:false
                         startCluster:currCluster
                         replyHandler:^(uint32_t startCluster, uint32_t length) {
            if (length) {
                currCluster = startCluster;
                nextChainLength = length;
            }
        }];

        if (nextChainLength) {
            return reply(nil, currCluster, nextChainLength);
        }

        if ((startBlockOffset == fatBlock.startOffset) && wrapAround)  {
            return reply(fs_errorForPOSIXError(ENOSPC), 0, 0);
        }
                                                          
        currCluster -= currCluster % [self clustersPerBlock];
        currCluster += [self clustersPerBlock];

        if (currCluster > self.fsInfo.maxValidCluster) {
            wrapAround = true;
            currCluster = FIRST_VALID_CLUSTER;
        }
    }
    
    // should never get here
    return reply(fs_errorForPOSIXError(ENOSPC), 0, 0);
}

@end
