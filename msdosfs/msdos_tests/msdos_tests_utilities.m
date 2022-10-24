//
//  msdos_tests_utilities.m
//  msdos_tests
//
//  Created by Tomer Afek on 27/06/2022.
//

#import "msdos_tests_utilities.h"

@implementation MsdosUnicodeComparator

+(MsdosUnicodeComparator*)newWithFs:(UVFSPluginFilesystem *)fs
{
    return [[self alloc] initWithFS:fs];
}

-(unsigned long)getTableSize
{
    // In msdosfs, we don't keep track of a conversion table, and we only support the first [0-0x100] characters
    return 0x100;
}

-(errno_t)convert:(char*)current converted:(char*)converted
{
    return CONV_UTF8ToLowerCase(current, converted);
}

-(int)compare:(char*)current converted:(char*)converted
{
    return strcmp(current, converted);
}

@end


@implementation MsdosSizesInfo

+(MsdosSizesInfo*)newWithFs:(UVFSPluginFilesystem *)fs
{
    MsdosSizesInfo* info = [[self alloc] initWithFS:fs];
    if (info) {
        NodeRecord_s* psParentRecord = GET_RECORD(fs.rootNode);
        FileSystemRecord_s* psFSRecord  = GET_FSRECORD(psParentRecord);
        [info setPsFSRecord:psFSRecord];
    }
    return info;
}

-(uint64_t)getClusterSize
{
    return CLUSTER_SIZE(self.psFSRecord);
}

-(uint64_t)getExpectedFaSizeForEmptyDir
{
    return [self getClusterSize];
}

-(uint64_t)getExpectedAllocSizeForFileBiggerThanOneCluster
{
    return 2 * [self getClusterSize];
}

-(uint64_t)getExpectedAllocSizeForFileSmallerThanOneCluster
{
    return [self getClusterSize];
}

@end


@implementation MsdosDiskImageInfo

+(MsdosDiskImageInfo*)newWithFs:(UVFSPluginFilesystem *)fs
{
    MsdosDiskImageInfo* info = [[self alloc] initWithFS:fs];
    if (info) {
        NodeRecord_s* psParentRecord = GET_RECORD(fs.rootNode);
        FileSystemRecord_s* psFSRecord  = GET_FSRECORD(psParentRecord);
        [info setPsFSRecord:psFSRecord];
    }
    return info;
}

-(uint64_t)getDirtyBitByteOffset
{
    uint64_t retVal;
    switch(self.psFSRecord->sFatInfo.uFatMask) {
        case FAT32_MASK:
            retVal = FAT32_DIRTY_BIT_BYTE_OFFSET_HEX;
            break;
        case FAT16_MASK:
            retVal = FAT16_DIRTY_BIT_BYTE_OFFSET_HEX;
            break;
        default: // FAT12
            /*
             * FAT12 doesn't have a dirty bit - shouldn't reach here.
             * (Shouldn't run tests which involve the dirty bit on FAT12).
             */
            retVal = 0;
            break;
    }
    return retVal;
}

-(uint8_t)getDirtyBitLocationInByte
{
    uint64_t retVal;
    switch(self.psFSRecord->sFatInfo.uFatMask) {
        case FAT32_MASK:
            retVal = FAT32_DIRTY_BIT_LOCATION_IN_BYTE;
            break;
        case FAT16_MASK:
            retVal = FAT16_DIRTY_BIT_LOCATION_IN_BYTE;
            break;
        default: // FAT12
            /*
             * FAT12 doesn't have a dirty bit - shouldn't reach here.
             * (Shouldn't run tests which involve the dirty bit on FAT12).
             */
            retVal = 0;
            break;
    }
    return retVal;
}

-(uint8_t)getDirtyBitDirtyValue
{
    return MSDOS_DIRTY_BIT_DIRTY_VALUE;
}

@end


@implementation MsdosMiscInfo

+(MsdosMiscInfo*)newWithFs:(UVFSPluginFilesystem *)fs
{
    MsdosMiscInfo* info = [[self alloc] initWithFS:fs];
    if (info) {
        NodeRecord_s* psParentRecord = GET_RECORD(fs.rootNode);
        FileSystemRecord_s* psFSRecord  = GET_FSRECORD(psParentRecord);
        [info setPsFSRecord:psFSRecord];
    }
    return info;
}

-(bool)isRootDirExtensible
{
    uint64_t retVal;
    switch(self.psFSRecord->sFatInfo.uFatMask) {
        case FAT32_MASK:
            retVal = true;
            break;
        case FAT16_MASK:
            // In FAT16 root dir cannot be extended
            retVal = false;
            break;
        default: // FAT12
             // In FAT12 root dir cannot be extended
            retVal = false;
            break;
    }
    return retVal;
}

-(bool)isDirtyBitTestable
{
    uint64_t retVal;
    switch(self.psFSRecord->sFatInfo.uFatMask) {
        case FAT32_MASK:
            retVal = true;
            break;
        case FAT16_MASK:
            retVal = true;
            break;
        default: // FAT12
             // FAT12 doesn't implement dirty bit
            retVal = false;
            break;
    }
    return retVal;
}

@end


@implementation MsdosAttrsInfo

-(uint32_t)getSupportedBSDFlagsMask
{
    return MSDOS_VALID_BSD_FLAGS_MASK;
}

-(bool)isAccessTimeSupported
{
    return false; // MSDOS only support access date, not time.
}

-(bool)isChangeDirTimeAttrsSupported
{
    // For dirs, MSDOS returns the time attrs of "." instead of the current node.
    return false;
}

-(int)getTimeGranularityInSeconds
{
    return MSDOS_TIME_GRANULARITY_IN_SECONDS;
}

-(NSDate*)getUnixOldestDateSupported
{
    return [NSDate dateWithTimeIntervalSince1970:MSDOS_OLDEST_SUPPORTED_DATE_UTC];
}

-(NSString*)getFsTypeName
{
    return [NSString stringWithUTF8String:MSDOS_FSTYPENAME];
}

@end


@implementation MsdosFactory

-(UnicodeComparator*)createUnicodeComparator
{
    MsdosUnicodeComparator* comparator = [MsdosUnicodeComparator newWithFs:self.fs];
    return comparator;
}

-(FileSystemSizesInfo*)createFileSystemSizesInfo
{
    MsdosSizesInfo* info = [MsdosSizesInfo newWithFs:self.fs];
    return info;
}

-(DiskImageInfo*)createDiskImageInfo
{
    MsdosDiskImageInfo* info = [MsdosDiskImageInfo newWithFs:self.fs];
    return info;
}

-(FileSystemMiscInfo*)createFileSystemMiscInfo
{
    FileSystemMiscInfo* info = [MsdosMiscInfo newWithFs:self.fs];
    return info;
}

-(AttrsInfo*)createAttrsInfo;
{
    MsdosAttrsInfo* info = [[MsdosAttrsInfo alloc] init];
    return info;
}

@end


@implementation MsdosSetupDelegate

-(instancetype)initWithSize:(NSString *)volumeSize
{
    self = [super init];
    if(self) {
        _volumeUtils = [[TestVolumeUtils alloc] initTestVolumeWithName:@"MS-DOS" size:volumeSize newfsPath:@"/sbin/newfs_msdos"];
    }
    return self;
}

-(void)deinit
{
    [self.volumeUtils clearTestVolume];
}

-(UVFSPluginFilesystem *)getFileSystem
{
    return [[UVFSPluginFilesystem alloc] initWithFSOps:&MSDOS_fsOps devicePath:_volumeUtils.devicePath volumeName:_volumeUtils.volumeName newfsPath:_volumeUtils.newfsPath];
}

-(UVFSPluginInterfacesFactory *)getFactory:(UVFSPluginFilesystem *)fs
{
    return [[MsdosFactory alloc] initWithFS:fs];
}

@end
