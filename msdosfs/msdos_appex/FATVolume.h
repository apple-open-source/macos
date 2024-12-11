/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef FATVolume_h
#define FATVolume_h

#import <Foundation/Foundation.h>
#import <FSKit/FSKit.h>
#import <stdatomic.h>

#import "utils.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(uint8_t, fatType) {
    FAT12,
    FAT16,
    FAT32,
    EXFAT
};


@interface FileSystemInfo : NSObject

@property fatType type;
@property uint32_t FATMask;

@property dirtyBitValue dirtyBitValue;

@property size_t dirBlockSize;
@property uint32_t bytesPerSector;
@property uint32_t bytesPerCluster;

@property uint32_t firstFreeCluster;
@property uint32_t firstClusterOffset; /* in sectors */
@property uint64_t maxValidCluster;
@property uint32_t firstDirBlockNum;

@property uint64_t freeClusters; // FAT32 / ExFAT only
@property uint32_t rootFirstCluster; // FAT32 / ExFAT only
@property uint8_t numOfFATs; // MSDOS only

@property uint32_t rootSector; // Offset in sectors from volume start to root dir start
@property (nonatomic,readonly) uint64_t rootDirBytes; // Root dir size in bytes (msdos only).

@property NSString *fsTypeName;
@property NSString *fsSubTypeName; // TBD - remove when we have new FSVolume properties
@property NSNumber *fsSubTypeNum;  // TBD - remove when we have new FSVolume properties
@property NSString *volumeLabel;
@property bool volumeLabelExists;

@property uint32_t serialNumber;
@property bool serialNumberExists;

@property FSVolumeIdentifier * _Nonnull uuid;
@property bool uuidExists;

@property uint64_t fatOffset; // Offset in bytes from volume start to active FAT
@property uint64_t fatSize; // Size of FAT in bytes

-(bool)isClusterValid:(uint32_t)cluster;

-(uint64_t)offsetForCluster:(uint64_t)cluster;
-(uint64_t)offsetForDirBlock:(uint64_t)dirBlock;

-(bool)isFAT12Or16;

-(uint64_t)fileSizeInBits; // TBD - remove when we have new FSVolume properties 

@end

/// Use an array of this object when reporting back allocated clusters
@interface AllocatedClustersInfo : NSObject

@property uint32_t startCluster;
@property uint32_t chainLength;

@end


@class DirNameCachePool;
@class DirNameCache;
@class ItemCache;
@class FATManager;
@class FATItem;
@class DirItem;
@class FileItem;
@class SymLinkItem;
@class DirEntryData;

@protocol FATOps <NSObject>

-(DirItem *)createDirItemWithParent:(FATItem * _Nullable)parentDir
					   firstCluster:(uint32_t)firstCluster
					   dirEntryData:(DirEntryData * _Nullable)dirEntryData
							   name:(nonnull NSString *)name
							 isRoot:(bool)isRoot;

-(FileItem *)createFileItemWithParent:(FATItem * _Nullable)parentDir
						 firstCluster:(uint32_t)firstCluster
						 dirEntryData:(DirEntryData * _Nullable)dirEntryData
                                 name:(nonnull NSString *)name;

-(SymLinkItem *)createSymlinkItemWithParent:(FATItem * _Nullable)parentDir
							   firstCluster:(uint32_t)firstCluster
							   dirEntryData:(DirEntryData * _Nullable)dirEntryData
                                       name:(nonnull NSString *)name
                              symlinkLength:(uint16_t)length;

-(void)FatMount:(FSTaskParameters *)options
   replyHandler:(void (^)(FSItem * _Nullable, NSError * _Nullable))reply;

-(void)getFreeClustersStats:(uint32_t *_Nonnull)freeClusters
               replyHandler:(void (^)(NSError * _Nullable error))reply;

-(void)calcNumOfDirEntriesForName:(struct unistr255)unistrName
                     replyHandler:(void (^)(NSError * _Nullable error, uint32_t numberOfEntries))reply;

-(void)nameToUnistr:(NSString *)name
			  flags:(uint32_t)flags
       replyHandler:(void (^)(NSError *error, struct unistr255 unistrName))reply;

-(NSError *)verifyFileSize:(uint64_t)fileSize;

-(bool)isOffsetInMetadataZone:(uint64_t)offset;

/* Per-FS syncing specifics go here, as msdos and ExFAT diverge */
-(NSError *)sync;

-(void)setVolumeLabel:(NSData *)name
              rootDir:(DirItem *)rootItem
         replyHandler:(void (^)(FSFileName * _Nullable newVolumeName,
                                NSError * _Nullable error))reply;

@end


@interface FATVolume : FSVolume <FSVolumeOperations, FSVolumeKernelOffloadedIOOperations, FSVolumeRenameOperations, FSVolumePreallocateOperations, FATOps>

@property FSBlockDeviceResource *resource;
@property DirNameCachePool *nameCachePool;
@property FileSystemInfo *systemInfo;
@property FATManager *fatManager;
@property ItemCache *itemCache;
@property DirItem *rootItem;

@property (nonatomic, readonly, getter=GetGMTDiffOffset) int gmtDiffOffset;
@property NSMutableArray<NSNumber*> *preAllocatedOpenFiles;
@property NSMutableArray<NSNumber*> *nextAvailableFileID;
@property NSMutableArray<NSNumber*> *openUnlinkedFiles;

-(instancetype)initWithResource:(FSResource *)resource
                       volumeID:(nonnull FSVolumeIdentifier *)volumeID
                     volumeName:(nonnull NSString *)volumeName;

-(NSError *)clearNewDirClustersFrom:(uint32_t)firstClusterToClear
							 amount:(uint32_t)numClustersToClear;
-(uint64_t)getFileID:(DirEntryData *)entryData;
-(uint64_t)getNextAvailableFileID;
- (FSItemGetAttributesRequest *)getAttrRequestForNewDirEntry;

@end


/// Operations based on the file system subtype (FAT12, 16, 32)
@interface FSOperations : NSObject

@property fatType fatType;

-(id)initWithType:(fatType)type;

/*
 * For FAT16/32, returns the amount of bytes it takes to
 * represent a cluster in the FAT.  For FAT12, returning 0.
 * (For FAT12 there is no such constant. This is a function of the cluster number).
 * NOTE: In case this method returns 0, the caller should call fatEntryOffsetForCluster
 * for each cluster number to get the desired offset, rather than incrementing the offset.
 */
-(uint32_t)numBytesPerClusterInFat;

-(uint32_t)fatEntryOffsetForCluster:(uint32_t)cluster;

-(uint32_t)getNextClusterFromEntryForCluster:(uint32_t)cluster
                                       entry:(uint8_t*)entry;

-(uint32_t)setFatEntryForCluster:(uint32_t)cluster
                           entry:(uint8_t*)entry
                       withValue:(uint32_t)value;

-(uint32_t)getDirtyBitCluster;

-(dirtyBitValue)getDirtyBitValueForEntry:(uint8_t *)dirtyBitEntry;

-(void)applyDirtyBitValueToEntry:(uint8_t *)dirtyBitEntry
                        newValue:(dirtyBitValue)newValue;

@end


NS_ASSUME_NONNULL_END
#endif /* FATVolume_h */
