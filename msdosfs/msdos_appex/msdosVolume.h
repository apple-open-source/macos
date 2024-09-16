/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef msdosVolume_h
#define msdosVolume_h

#import <Foundation/Foundation.h>
#import "FATVolume.h"

#define MSDOS_FAT_BLOCKSIZE(offset, fatSize)    (((offset) + FAT_BLOCKSIZE > (fatSize)) ? (fatSize) - (offset) : FAT_BLOCKSIZE)

#define MSDOSFS_XATTR_VOLUME_ID_NAME    "com.apple.filesystems.msdosfs.volume_id"

NS_ASSUME_NONNULL_BEGIN

@interface msdosVolume : FATVolume <FATOps, FSVolumeXattrOperations>

@property bool isVolumeDirty;
@property fatType type;

-(int)ScanBootSector;

-(instancetype)initWithResource:(FSResource *)resource
                       volumeID:(FSVolumeIdentifier *)volumeID
                     volumeName:(NSString *)volumeName;

@end


@interface FileSystemInfo()

@property uint32_t rootDirSize; // Root dir size in sectors
@property uint64_t metaDataZoneSize;
@property uint32_t fsInfoSectorNumber; // FAT32 only, 0 for FAT12/16
@property NSMutableData *fsInfoSector; // FAT32 only

-(instancetype)initWithBlockDevice:(FSBlockDeviceResource *)device;

@end

@interface msdosVolume()

@property FSOperations * fsOps;

@end

NS_ASSUME_NONNULL_END

#endif /* msdosVolume_h */
