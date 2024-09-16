/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "FATManager.h"
#import "DirBlock.h"
#import "DirItem.h"
#import "utils.h"

@interface DirBlock()

@property DirItem               *dir;
@property NSMutableData         *data;
@property size_t                size;

@end

@implementation DirBlock

-(instancetype)initInDir:(DirItem *)dirItem
{
	self = [super init];
	if (self) {
        self.dir = dirItem;
		self.offsetInVolume = 0;
        self.size = [dirItem getDirBlockSize];
        self.data = [NSMutableData dataWithLength:self.size];
        dispatch_semaphore_wait(self.dir.sem, DISPATCH_TIME_FOREVER);
	}
	return self;
}

-(void)releaseBlock
{
    dispatch_semaphore_signal(self.dir.sem);
}

-(NSError *)readDirBlockNum:(uint64_t)dirBlockNumberInVolume
{
    uint64_t dirBlockOffsetInVolume = 0;
    if ([self.dir isFat1216RootDir]) {
        /* For FAT12/16, we have a single dir block for the root directory, and we know its offset. */
        dirBlockOffsetInVolume = self.dir.volume.systemInfo.rootSector * self.dir.volume.systemInfo.bytesPerSector;
    } else {
        dirBlockOffsetInVolume = [self.dir.volume.systemInfo offsetForDirBlock:dirBlockNumberInVolume];
    }
    NSError *error = [Utilities syncMetaReadFromDevice:self.dir.volume.resource
                                                  into:self.data.mutableBytes
                                            startingAt:dirBlockOffsetInVolume
                                                length:self.size];
	if (error == nil) {
		self.offsetInVolume = dirBlockOffsetInVolume;
	}
	return error;
}

-(NSError *)readRelativeDirBlockNum:(uint32_t)dirBlockIdxInDir
{
    __block NSError *error = 0;
    __block uint32_t curNextCluster = 0;
    __block uint32_t numContigClusters = 0;
    uint32_t dirBlockNumInVolume = 0;
    uint32_t curStartCluster = self.dir.firstCluster;
    uint32_t curDirBlockIdx = 0;
    size_t dirBlockSize = self.dir.volume.systemInfo.dirBlockSize;
    size_t clusterSize = self.dir.volume.systemInfo.bytesPerCluster;
    size_t dirBlocksPerCluster = clusterSize / dirBlockSize;

    if ([self.dir isFat1216RootDir]) {
        /* For FAT12/16 root dir, we don't care about the "dirBlockNumInVolume",
         because we have a single dir block with fixed offset. */
        return [self readDirBlockNum:0];
    }

    while ([self.dir.volume.systemInfo isClusterValid:curStartCluster]) {
        [self.dir.volume.fatManager getContigClusterChainLengthStartingAt:curStartCluster
                                                             replyHandler:^(NSError * _Nullable fatError,
                                                                            uint32_t length,
                                                                            uint32_t nextClusterInChain) {
            if (fatError) {
                error = fatError;
            } else {
                numContigClusters = length;
                curNextCluster = nextClusterInChain;
            }
        }];

        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to get the next cluster(s). Error = %@.", __func__, error);
            return error;
        }

        if ((curDirBlockIdx <= dirBlockIdxInDir) &&
            (curDirBlockIdx + numContigClusters * dirBlocksPerCluster > dirBlockIdxInDir)) {
            /* We've got to a cluster we're interested in. */
            dirBlockNumInVolume = (uint32_t)(curStartCluster * dirBlocksPerCluster + (dirBlockIdxInDir - curDirBlockIdx));
            break;
        }
        curDirBlockIdx += numContigClusters * dirBlocksPerCluster;
        curStartCluster = curNextCluster;
    }

    if (dirBlockNumInVolume == 0) {
        /* Couldn't find the given dir block index (0 is an invalid value when we're not in FAT12/16 root dir). */
        os_log_fault(fskit_std_log(), "%s: Couldn't find dir block index %u. Dir Size = %llu.", __func__, dirBlockIdxInDir, [self.dir getDirSize]);
        return fs_errorForPOSIXError(EFAULT);
    }

    /* Succussfully converted the idx to absolute number. Now perform the actual read. */
    return [self readDirBlockNum:dirBlockNumInVolume];
}


-(void *)getBytesAtOffset:(uint64_t)offsetInDirBlock
{
    if (offsetInDirBlock >= self.size) {
        return NULL;
    }
	return (void *)((uint8_t *)self.data.bytes + offsetInDirBlock);
}

-(NSError *)setBytes:(NSData *)data
            atOffset:(uint64_t)offsetInDirBlock
{
    if (offsetInDirBlock + data.length > self.size) {
        return fs_errorForPOSIXError(EINVAL);
    }

    memcpy((uint8_t *)self.data.mutableBytes + offsetInDirBlock, (uint8_t*)data.bytes, data.length);
    return nil;
}

-(NSError *)writeToDisk
{
	return [Utilities metaWriteToDevice:self.dir.volume.resource
                                   from:(void *)self.data.bytes
                             startingAt:self.offsetInVolume
                                 length:self.size];
}

-(NSError *)writeToDiskFromOffset:(uint64_t)offsetInDirBlock
						   length:(uint64_t)lengthToWrite
{
	if (offsetInDirBlock + lengthToWrite > self.size) {
		return fs_errorForPOSIXError(EINVAL);
	}
	// TODO: should be subblock meta write. Otherwise there's no reason for calling this method.
//	return [Utilities metaWriteToDevice:self.dir.volume.resource
//							       from:(uint8_t *)(self.data.bytes) + offsetInDirBlock
//						     startingAt:self.offsetInVolume + offsetInDirBlock
//								 length:lengthToWrite];
    return [self writeToDisk];
}

@end
