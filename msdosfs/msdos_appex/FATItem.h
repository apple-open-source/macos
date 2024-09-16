/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef FATItem_h
#define FATItem_h

#import <Foundation/Foundation.h>
#import <FSKit/FSKit.h>

#import "FATVolume.h"
#import "utils.h"

typedef NS_ENUM(uint8_t, dirEntryType) {
	FATDirEntryDeleted   = 0,
	FATDirEntryEmpty,
	FATDirEntryFound,
	FATDirEntryVolName,
	FATDirEntryUnknown,
	FATDirEntryStart,
};

typedef NS_OPTIONS(uint32_t, iterateDirOptions) {
    iterateDirOptionSkipDotEntries  = 1, // Don't call the reply block for '.' and '..' dir entries
};

typedef NS_ENUM(uint8_t, iterateDirStatus) {
	iterateDirContinue = 0,
	iterateDirStop
};

NS_ASSUME_NONNULL_BEGIN

@interface DirEntryData : NSObject

@property NSMutableData *data;
@property uint16_t numberOfDirEntries;
@property uint64_t firstEntryOffsetInDir;

+(instancetype)dynamicCast:(id)candidate;
-(instancetype)initWithData:(NSData *)data;
-(void)getAccessTime:(struct timespec *)tp;
-(void)setAccessTime:(struct timespec *)tp;
-(void)getModifyTime:(struct timespec *)tp;
-(void)setModifyTime:(struct timespec *)tp;
-(void)getChangeTime:(struct timespec *)tp;
-(void)setChangeTime:(struct timespec *)tp;
-(void)getBirthTime:(struct timespec *)tp;
-(void)setBirthTime:(struct timespec *)tp;
-(uint32_t)getFirstCluster:(id)systemInfo;
-(void)setFirstCluster:(uint32_t)firstCluster
        fileSystemInfo:(FileSystemInfo *)systemInfo;
-(uint8_t *)getName;
-(void)setName:(uint8_t *)name;
-(uint64_t)getSize;
-(void)setSize:(uint64_t)size;
-(uint64_t)getValidDataLength;
-(void)setValidDataLength:(uint64_t)validDataLength;
-(void)setArchiveBit;

-(uint64_t)calcFirstEntryOffsetInVolume:(FileSystemInfo *)systemInfo;

@property (nonatomic,readonly) FSItemType type;
@property (nonatomic,readonly) uint32_t   bsdFlags;

@end


@interface FATItem : FSUnaryItem

@property FATVolume *volume;

@property NSString *name;
@property bool isDeleted;

@property FATItem * _Nullable parentDir;

@property (nonatomic) uint32_t firstCluster;
@property (nonatomic) uint32_t firstClusterInLastAllocation;
@property (nonatomic) uint32_t firstClusterIndexInLastAllocation;
@property uint32_t lastCluster;
@property uint32_t numberOfClusters;

@property DirEntryData * _Nullable entryData;

+(instancetype)dynamicCast:(id)candidate;

-(instancetype)initInVolume:(FATVolume *)volume
                      inDir:(FATItem * _Nullable)parentDir
                 startingAt:(uint32_t)firstCluster
                   withData:(DirEntryData * _Nullable)entryData
                    andName:(nonnull NSString *)name
                     isRoot:(bool)isRoot;

-(uint64_t)getFileID;

-(FSItemAttributes *)getAttributes:(nonnull FSItemGetAttributesRequest *)desired;

-(NSError *)setAttributes:(nonnull FSItemSetAttributesRequest *)newAttributes;

-(NSError *)flushDirEntryData;

-(void)setDeleted;

-(NSError *)reclaim;

/*
 * Symlinks have a non constant block size, so the implementation slightly
 * differs from the implementation for directory, where we should purge all
 * the blocks we get from the FATManager up to a known size.
 */
-(void)purgeMetaBlocksFromCache:(void (^)(NSError * _Nullable error))reply;

@end


@interface SymLinkItem: FATItem

@property uint16_t symlinkLength;

+(NSError *)createSymlinkFromContent:(nonnull NSData *)contents
                            inBuffer:(nonnull NSMutableData *)buffer;

+(void)verifyAndGetLink:(NSMutableData *)linkData
           replyHandler:(nonnull void (^)(NSError * _Nullable, NSString * _Nullable linkStr))reply;

@end


@interface FileItem : FATItem

@property bool isPreAllocated;
@property uint64_t writeCounter;
@property NSMutableDictionary *blockmapRequests;

-(instancetype)initInVolume:(FATVolume *)volume
                      inDir:(FATItem * _Nullable)parentDir
                 startingAt:(uint32_t)firstCluster
                   withData:(DirEntryData * _Nullable)entryData
                    andName:(nonnull NSString *)name;

-(uint64_t)maxFileSize;

/**
 Truncate the file to the given size.
 NOTE: this method only allocates the clusters. It doesn't update the entry data with the new file size, nor flushes it to disk.
 It does update the entry data with the first cluster, if needed.
 @param newSize The desired new file size.
 @param allowPartial Whether we allow to only allocate part of the needed clusters (if we have no space for all).
    (only relevant in case newSize > current file size).
 @param mustBeContig Whether we require new allocted space to be contiguous on disk or not.
 In case of success, returns nil, o.w. returns the error.
 */
-(NSError *)truncateTo:(uint64_t)newSize
          allowPartial:(bool)allowPartial
          mustBeContig:(bool)mustBeContig;

/**
 Blockmap the given range in the file.
 @param range The range in file to blockmap.
 @param startIO Whether it's the first blockmapFile call for this range.
 @param flags blockmapFile flags.
 @param operationID a unique ID for this blockmapFile operation. Used for saving some context to use in endIO.
 @param reply In case of an error, calling reply(error). In case of success, calling reply(nil).
 */
-(void)blockmapRange:(NSRange)range
             startIO:(bool)startIO
               flags:(FSBlockmapFlags)flags
         operationID:(uint64_t)operationID
         usingBlocks:(FSExtentPacker)packer
        replyHandler:(nonnull void (^)(NSError * _Nullable error))reply;

/**
 endIO the given range in the file.
 @param range The range in file to end the IO for.
 @param ioStatus Whether the IO succeeded or not.
 @param flags endIO flags.
 @param operationID the operationID used for the corresponding blockmapFile operation.
 */
-(NSError *)endIOOfRange:(NSRange)range
                  status:(int)ioStatus
                   flags:(FSBlockmapFlags)flags
             operationID:(uint64_t)operationID;

/**
 Fetch the relevant extents of the given file.
 @param startOffset The offset in file to start fetching the extents from.
 @param endOffset The offset in file to stop fetching the extents at.
    (we stop fetching extents when we reach this limit, even if we haven't got to endOffset).
 @param lastValidOffset maximal offset for which we return "data-fill" extents.
    Beyond that offset, we start returning "zero-fill" extents.
 @param packer The extent packer block
 @param reply In case of an error, calling reply(error, nil, 0). In case of success, calling reply(nil, extentsData, numOfExtentsFetched).
 */
-(void)fetchFileExtentsFrom:(uint64_t)startOffset
                         to:(uint64_t)endOffset
            lastValidOffset:(uint64_t)lastValidOffset
                usingBlocks:(FSExtentPacker)packer
               replyHandler:(nonnull void (^)(NSError * _Nullable error))reply;

/**
 Preallocate more bytes to the file allocated space.
 @param size Size in bytes to be preallocated.
 @param allowPartial Whether we allow to only preallocate part of the needed clusters (if we have no space for all).
    (only relevant in case newSize > current file size).
 @param mustBeContig Whether we require preallocted sace to be contiguous on disk or not.
    Beyond that offset, we start returning "zero-fill" extents.
 @param reply In case of an error, calling reply(error, 0). In case of success, calling reply(nil, AllocatedSize) when AllocatedSize can be greater then
    the asked size if size was rounded up to cluster size.
 */
-(void)preallocate:(uint64_t)size
      allowPartial:(bool)allowPartial
      mustBeContig:(bool)mustBeContig
      replyHandler:(nonnull void (^)(NSError * _Nullable error,
                                     uint64_t allocatedSize))reply;

/**
 If the preallocation status of the item has changed, update the number of the
 volume's preallocated open files and the file's own preallocation status.
 @param isPreallocated The new preallocation status of the item.
 */
-(void)setPreAllocated:(bool)isPreallocated;

@end

NS_ASSUME_NONNULL_END

#endif /* FATItem_h */
