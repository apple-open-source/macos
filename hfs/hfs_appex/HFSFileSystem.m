//
//  HFSFileSystem.m
//
//  Created by Adam Hijaze on 24/08/2022.
//

#import <FSKit/FSResource.h>
#import <FSKit/FSBlockDeviceResource_private.h>

#include <sys/loadable_fs.h>

#import  "HFSFileSystem.h"


@implementation HFSFileSystem

/**
 @method getVolumeName
 @abstract Reads volume name for the provided HFS master directory block
 @param fd Device file descriptor
 @param masterBlock Device master directory block
 @param reply Reports volume name, or any error while reading the volume name
 */
-(void)getVolumeName:(int)fd
         masterBlock:(HFSMasterDirectoryBlock *)masterBlock
               reply:(nonnull void (^)(int, NSString * _Nullable))reply
{
    NSString *volName = nil;
    HFSMasterDirectoryBlock * mdbPtr;
    HFSPlusVolumeHeader *volHdrPtr;
    int result = FSMatchNotRecognized;
    u_int32_t allocationBlockSize = 0;
    u_int32_t firstAllocationBlock = 0;
    u_int32_t startBlock = 0;
    u_int32_t blockCount = 0;
    u_char volnameUTF8[kHFSPlusMaxFileNameBytes];
    
    mdbPtr = (HFSMasterDirectoryBlock *) masterBlock;
    volHdrPtr = (HFSPlusVolumeHeader *) masterBlock;
    
    // Get classic HFS volume name (from MDB)
    if (OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord &&
        OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) != kHFSPlusSigWord) {
        result = FSMatchRecognized;
    // Get HFS Plus volume name (from Catalog)
    } else if ((OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSPlusSigWord)  ||
               (OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSXSigWord)  ||
               (OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord &&
                OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) == kHFSPlusSigWord)) {
        off_t startOffset;
        
        if (OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSSigWord) {
            // Embedded volume, first find offset
            if (OSSwapBigToHostInt16(mdbPtr->drSigWord) != kHFSSigWord) {
                result = FSMatchRecognized;
                goto exit;
            }

            allocationBlockSize = OSSwapBigToHostInt32(mdbPtr->drAlBlkSiz);
            firstAllocationBlock = OSSwapBigToHostInt16(mdbPtr->drAlBlSt);

            if (OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) != kHFSPlusSigWord) {
                result = FSMatchRecognized;
                goto exit;
            }

            startBlock = OSSwapBigToHostInt16(mdbPtr->drEmbedExtent.startBlock);
            blockCount = OSSwapBigToHostInt16(mdbPtr->drEmbedExtent.blockCount);

            startOffset = ((u_int64_t)startBlock * (u_int64_t)allocationBlockSize) +
                ((u_int64_t)firstAllocationBlock * (u_int64_t)kMDBSize);
        } else {
            startOffset = 0;
        }
        
        result = hfs_GetNameFromHFSPlusVolumeStartingAt(fd, startOffset, volnameUTF8, OSSwapBigToHostInt32(volHdrPtr->blockSize));
    } else {
        result = FSMatchNotRecognized;
    }

exit:
    if (result == FSUR_IO_SUCCESS) {
        volName = [NSString stringWithUTF8String:volnameUTF8];
        result = FSMatchUsable;
    }
    reply(result, volName);
}

- (void)didFinishLoading
{
    os_log_info(fskit_std_log(), "%s: Finished loading", __FUNCTION__);
}

-(void)didFinishLaunching
{
    os_log_info(fskit_std_log(), "%s: Finished launching", __FUNCTION__);
}

-(void)loadVolume:(FSResource *)device
            reply:(nonnull void (^)(FSVolume * _Nullable, NSError * _Nullable))reply
{
    return (void)reply(nil, fs_errorForPOSIXError(ENOTSUP));
}

-(void)probeResource:(FSResource *)resource
        replyHandler:(nonnull void (^)(FSProbeResult *result,
                                       NSError * _Nullable error))reply
{
    int result;
    FSProbeResult *probeResult;
    void *masterBlockBuffer = NULL;
    __block NSError *error = nil;
    __block NSString *volumeName = nil;
    __block BOOL nameReadSucc = TRUE;
    NSUUID *volUUID = nil;
    unsigned long blockSize = 0;
    FSMatchResult match = FSMatchNotRecognized;
    FSBlockDeviceResource *device;
    HFSMasterDirectoryBlock *masterBlock = NULL;
    
    device = [FSBlockDeviceResource dynamicCast:resource];
    if (device == nil) {
        os_log_fault(fskit_std_log(), "%s: Given device is not a block device", __FUNCTION__);
        return reply([FSProbeResult resultWithResult:FSMatchNotRecognized
                                                name:nil
                                         containerID:nil], nil);
    }
    
    masterBlock = malloc(kMDBSize);
    if ( masterBlock == NULL ) {
        error = fs_errorForPOSIXError(ENOMEM);
        os_log(fskit_std_log(), "%s: Failed to allocate masterBlock", __FUNCTION__);
        goto exit;
    }
    
    blockSize = device.blockSize;
    
    if (blockSize == 0 || blockSize > kMaxLogicalBlockSize) {
        error = fs_errorForPOSIXError(ENXIO);
        os_log(fskit_std_log(), "%s: Invalid block size (%lu)", __FUNCTION__, blockSize);
        goto exit;
    }

    if (blockSize > kMDBSize) {
        masterBlockBuffer = malloc(blockSize);
        if (masterBlockBuffer == NULL) {
            error = fs_errorForPOSIXError(ENOMEM);
            os_log(fskit_std_log(), "%s: Failed to allocate masterBlockBuffer", __FUNCTION__);
            goto exit;
        }
    } else {
        masterBlockBuffer = (void*) masterBlock;
    }
    
    // Read VolumeHeader from offset 1024
    off_t   uVolHdrOffset  = 1024;
    off_t   uBlockNum      = uVolHdrOffset / blockSize;
    off_t   uOffsetInBlock = uVolHdrOffset % blockSize;
    
    [device synchronousReadInto:masterBlockBuffer
                     startingAt:uBlockNum * blockSize
                         length:blockSize
                          reply:^(size_t actuallyRead, NSError * _Nullable innerError) {
        if (innerError) {
            error = innerError;
            os_log(fskit_std_log(), "%s: Falied to read Master Directory Block, error (%ld)", __FUNCTION__, (long)innerError.code);
        } else if (actuallyRead < uOffsetInBlock + kMDBSize) {
            error = fs_errorForPOSIXError(EIO);
            os_log_error(fskit_std_log(), "%s: Expected to read %lld bytes, read %lu", __FUNCTION__, kMDBSize + uOffsetInBlock, actuallyRead);
        }
    }];
     
    if (error) {
        goto exit;
    }

    if (blockSize > kMDBSize) {
        memcpy(masterBlock, masterBlockBuffer + uOffsetInBlock, kMDBSize);
    }
    
    // Validate Signature
    uint32_t drSigWord = SWAP_BE16(masterBlock->drSigWord);
    if ((drSigWord != kHFSPlusSigWord) &&
        (drSigWord != kHFSXSigWord)) {
        os_log(fskit_std_log(), "%s: Invalid volume signature", __FUNCTION__);
        goto exit;
    }
    
    // Get Volume UUID
    volUUID_t sVolUUID;
    result = hfs_GetVolumeUUIDRaw(device.fileDescriptor, &sVolUUID, (int)device.blockSize);
    if (result != FSUR_IO_SUCCESS) {
        error = fs_errorForPOSIXError(EIO);
        os_log_error(fskit_std_log(), "%s: Failed to get volume UUID", __FUNCTION__);
        goto exit;
    }
    
    // Get Volume name
    [self getVolumeName:device.fileDescriptor
            masterBlock:masterBlock
                  reply:^(int res, NSString * _Nullable volName){
        if (res == FSMatchUsable) {
            volumeName = volName;
        } else {
            nameReadSucc = FALSE;
            error = (res == FSMatchRecognized) ? nil : fs_errorForPOSIXError(ENOMEM);
            os_log_error(fskit_std_log(), "%s: Failed to read volume name", __FUNCTION__);
        }
    }];
    
    if (!nameReadSucc) {
        goto exit;
    }
    
    volUUID = [volUUID initWithUUIDBytes:sVolUUID.uuid];
    match = FSMatchUsable;
    
exit:
    if (blockSize > kMDBSize) {
        free(masterBlockBuffer);
    }
    
    if (masterBlock) {
        free(masterBlock);
    }
    probeResult = [FSProbeResult resultWithResult:match
                                             name:volumeName
                                      containerID:volUUID.fs_containerIdentifier];

    return reply(probeResult, error);
}

-(void)checkResource:(FSResource *)resource
             options:(FSTaskOptionsBundle *)options
          connection:(FSMessageConnection *)connection
              taskID:(NSUUID *)taskID
            progress:(NSProgress *)progress
        replyHandler:(void (^)(NSError * _Nullable))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP));
}

-(void)formatResource:(FSResource *)resource
              options:(FSTaskOptionsBundle *)options
           connection:(FSMessageConnection *)connection
               taskID:(NSUUID *)taskID
             progress:(NSProgress *)progress
         replyHandler:(void (^)(NSError * _Nullable))reply
{
    reply(fs_errorForPOSIXError(ENOTSUP));
}

@end
