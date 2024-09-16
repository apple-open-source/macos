/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <FSKit/FSVolume.h>
#import <machine/endian.h>
#import <sys/disk.h>

#import "FATManager.h"
#import "msdosVolume.h"
#import "msdosItem.h"
#import "bootsect.h"
#import "direntry.h"
#import "util.h"

NS_ASSUME_NONNULL_BEGIN

@implementation msdosVolume

-(instancetype)initWithResource:(FSResource *)resource
                       volumeID:(FSVolumeIdentifier *)volumeID
                     volumeName:(NSString *)volumeName

{
    self = [super initWithResource:resource
                          volumeID:volumeID
                        volumeName:volumeName];
    if (!self) {
        goto exit;
    }

    os_log_info(fskit_std_log(), "Hello MSDOS volume");

exit:
	return self;
}

/// To be called during load. We assume we already have a device at this point
/// Read the bootsector, do some sanity checks and retrieve the volume's name
/// and UUID.
/// @return error code, 0 on success
-(int)ScanBootSector
{
    NSMutableData *readBuffer = [[NSMutableData alloc] initWithLength:self.systemInfo.dirBlockSize];
    struct byte_bpb710 *b710 = NULL;
    struct extboot *extboot = NULL;
    struct byte_bpb50 *b50 = NULL;
    union bootsector *boot = NULL;
    uint32_t reservedSectors = 0;
    uint32_t bytesPerSector = 0;
    uint32_t rootEntryCount = 0;
    uint32_t totalSectors = 0;
    uint32_t fatSectors = 0;

    bytesPerSector = (uint32_t)self.systemInfo.dirBlockSize;

    /* read the boot sector from the device */
    NSError *nsError = [Utilities syncReadFromDevice:[self resource]
                                                into:readBuffer.mutableBytes
                                          startingAt:0
                                              length:bytesPerSector];
    if (nsError) {
        return (int)[nsError code];
    }

    boot = (union bootsector*)readBuffer.bytes;
    b50 = (struct byte_bpb50 *)boot->bs50.bsBPB;
    b710 = (struct byte_bpb710 *)boot->bs710.bsBPB;

    if (boot->bs50.bsJump[0] != 0xE9 &&
        boot->bs50.bsJump[0] != 0xEB) {
        os_log_error(fskit_std_log(), "%s: Invalid jump signature (0x%02X)", __func__, boot->bs50.bsJump[0]);
        return EINVAL;
    }
    if ((boot->bs50.bsBootSectSig0 != BOOTSIG0) ||
        (boot->bs50.bsBootSectSig1 != BOOTSIG1)) {
        /*
         * Not returning an error here as some volumes might have an unexpected
         * signature but are mounted on Windows systems and using our msdos kext.
         */
        os_log_error(fskit_std_log(), "%s: Invalid boot signature (0x%02X 0x%02X)",
                     __func__, boot->bs50.bsBootSectSig0, boot->bs50.bsBootSectSig1);
    }

    self.systemInfo.bytesPerSector = getuint16(b50->bpbBytesPerSec);
    self.systemInfo.bytesPerCluster = self.systemInfo.bytesPerSector * b50->bpbSecPerClust;

    /* Set dir block size. */
#if TARGET_OS_OSX
    self.systemInfo.dirBlockSize = MIN(self.systemInfo.bytesPerCluster, MSDOS_MAX_DIR_BLOCK_SIZE_MACOS);
#else // iOS
    // For iOS we currently always set dirBlockSize = sectorSize, because we don't do any sub-block
    // writes, and we don't want to write a big dir block every time we need to write one sector.
    self.systemInfo.dirBlockSize = self.systemInfo.bytesPerSector;
#endif /* TARGET_OS_OSX */

    reservedSectors = getuint16(b50->bpbResSectors);
    rootEntryCount = getuint16(b50->bpbRootDirEnts);

    /*
     * Sanity checks:
     * - logical sector size: == device's current sector size
     * - sectors per cluster: power of 2, >= 1
     * - number of sectors: >= 1
     * - number of FAT sectors > 0 (too large values handled later)
     */
    if (bytesPerSector != self.systemInfo.bytesPerSector) {
        os_log_error(fskit_std_log(), "%s: Logical sector size (%u) != physical sector size (%u)",
                     __func__, self.systemInfo.bytesPerSector, bytesPerSector);
        return EINVAL;
    }

    if (b50->bpbSecPerClust == 0 || b50->bpbSecPerClust & (b50->bpbSecPerClust - 1)) {
        os_log_error(fskit_std_log(), "%s: Invalid sectors per cluster (%u)", __func__, b50->bpbSecPerClust);
        return EINVAL;
    }

    totalSectors = (getuint16(b50->bpbSectors) == 0) ? getuint32(b50->bpbHugeSectors) :
        getuint16(b50->bpbSectors);
    if (totalSectors == 0) {
        if ((*((uint8_t*)boot + 0x42) == 0x42) &&
            (*((uint64_t*)boot + 0x52) != 0)) {
            os_log_error(fskit_std_log(), "%s: Encountered a special FAT where total sector location is 64bit. Not supported", __func__);
        }
        os_log_error(fskit_std_log(), "%s: Invalid total sectors (0)", __func__);
        return EINVAL;
    }

    fatSectors = (getuint16(b50->bpbFATsecs) == 0) ? getuint32(b710->bpbBigFATsecs) :
        getuint16(b50->bpbFATsecs);
    if (fatSectors == 0) {
        os_log_error(fskit_std_log(), "%s: Invalid sectors per FAT (0)", __func__);
        return EINVAL;
    }

    /*
     * Need to figure out how many clusters are there and whether the volume is
     * a FAT12/16/32 one.
     */
    self.systemInfo.rootSector = reservedSectors + b50->bpbFATs * fatSectors;
    self.systemInfo.rootDirSize = (rootEntryCount * sizeof(struct dosdirentry) + bytesPerSector - 1) / bytesPerSector;
    self.systemInfo.firstClusterOffset = self.systemInfo.rootSector + self.systemInfo.rootDirSize;
    self.systemInfo.metaDataZoneSize = self.systemInfo.rootSector * self.systemInfo.bytesPerSector;
    self.systemInfo.firstDirBlockNum = FIRST_VALID_CLUSTER * (self.systemInfo.bytesPerCluster / self.systemInfo.dirBlockSize);

    if ((fatSectors > totalSectors) ||
        (self.systemInfo.rootSector < fatSectors) ||
        (self.systemInfo.firstClusterOffset + b50->bpbSecPerClust > totalSectors)) {
        /* Seems like there isn't room even for a single cluster */
        os_log_error(fskit_std_log(), "%s: Invalid configuration, no root for clusters", __func__);
        return EINVAL;
    }

    self.systemInfo.maxValidCluster = (uint32_t)((totalSectors - self.systemInfo.firstClusterOffset) / b50->bpbSecPerClust + 1);

    /* Figure out FAT type, verify max cluster according to it */
    uint32_t calculatedNumClusters = fatSectors * bytesPerSector;
    self.systemInfo.fsInfoSectorNumber = 0; // Make sure it's 0 for FAT-12/16
    if (self.systemInfo.maxValidCluster < (RESERVED_CLUSTER_RANGE & MASK_12BIT)) {
        self.systemInfo.FATMask = MASK_12BIT;
        self.systemInfo.type = FAT12;
        self.systemInfo.fsSubTypeName = @"fat12";
        self.systemInfo.fsSubTypeNum = [NSNumber numberWithInt:0];
        calculatedNumClusters = calculatedNumClusters * 2 / 3;
    } else if (self.systemInfo.maxValidCluster < (RESERVED_CLUSTER_RANGE & MASK_16BIT)) {
        self.systemInfo.FATMask = MASK_16BIT;
        self.systemInfo.type = FAT16;
        self.systemInfo.fsSubTypeName = @"fat16";
        self.systemInfo.fsSubTypeNum = [NSNumber numberWithInt:1];
        calculatedNumClusters /= 2;
    } else if (self.systemInfo.maxValidCluster < (RESERVED_CLUSTER_RANGE & MASK_32BIT)) {
        self.systemInfo.FATMask = MASK_32BIT;
        self.systemInfo.type = FAT32;
        self.systemInfo.fsSubTypeName = @"fat32";
        self.systemInfo.fsSubTypeNum = [NSNumber numberWithInt:2];
        calculatedNumClusters /= 4;
        self.systemInfo.rootFirstCluster = getuint32(b710->bpbRootClust);
        self.systemInfo.fsInfoSectorNumber = getuint16(b710->bpbFSInfo);
    }  else {
        os_log_error(fskit_std_log(), "%s: Clusters number is too large (0x%llx)", __func__, self.systemInfo.maxValidCluster + 1);
        return EINVAL;
    }

    /*
     * Now verify that max cluster doesn't exceed the number of clusters we
     * calculated according to the FAT type
     */
    if (self.systemInfo.maxValidCluster >= calculatedNumClusters) {
        os_log_debug(fskit_std_log(), "%s: max cluster exceeds FAT capacity (%llu, %u)",
                     __func__, self.systemInfo.maxValidCluster, calculatedNumClusters);
        self.systemInfo.maxValidCluster = calculatedNumClusters - 1;
    }
    /* Compare to the value ioctl'd by the resource */
    if (self.resource.blockCount < totalSectors) {
        uint32_t tmpMaxCluster;

        if (self.systemInfo.firstClusterOffset +  b50->bpbSecPerClust > self.resource.blockCount) {
            os_log_error(fskit_std_log(), "%s: device sector count (%llu) is too small, no room for clusters",
                         __func__, self.resource.blockCount);
            return EINVAL;
        }
        tmpMaxCluster = (uint32_t)((self.resource.blockCount - self.systemInfo.firstClusterOffset) / b50->bpbSecPerClust + 1);
        if (tmpMaxCluster < self.systemInfo.maxValidCluster) {
            os_log_debug(fskit_std_log(), "%s: device sector count (%llu) us less than volume sector count (%u), limiting max cluster to %u (was %llu)",
                         __func__, self.resource.blockCount, totalSectors, tmpMaxCluster, self.systemInfo.maxValidCluster);
            self.systemInfo.maxValidCluster = tmpMaxCluster;
        } else {
            os_log_debug(fskit_std_log(), "%s: device sector count (%llu) is less than volume sector count (%u)",
                         __func__, self.resource.blockCount, totalSectors);
        }
    }

    self.systemInfo.fatOffset = getuint16(b50->bpbResSectors) * bytesPerSector;
    self.systemInfo.fatSize = fatSectors * bytesPerSector;
    self.systemInfo.numOfFATs = b50->bpbFATs;

    if (self.systemInfo.FATMask == MASK_32BIT) {
        extboot = (struct extboot*)boot->bs710.bsExt;
    } else {
        extboot = (struct extboot*)boot->bs50.bsExt;
    }

    /* Make sure the volume has ext boot */
    if (extboot->exBootSignature == EXBOOTSIG) {
        self.systemInfo.serialNumber =
             extboot->exVolumeID[0]        |
            (extboot->exVolumeID[1] << 8)  |
            (extboot->exVolumeID[2] << 16) |
            (extboot->exVolumeID[3] << 24);
        self.systemInfo.serialNumberExists = true;

        /* Generate UUID */
        if (self.systemInfo.serialNumber != 0) {
            unsigned char uuidBytes[16] = {0};
            self.systemInfo.uuid = [[FSVolumeIdentifier alloc] initWithUUID:[[NSUUID alloc] initWithUUIDBytes:uuidBytes]];// [[NSData alloc] initWithBytes:uuidBytes length:16];
            self.systemInfo.uuidExists = true;
        }
    }

    /* For FAT32, try to read FSInfo sector and get the free cluster count */
    if (self.systemInfo.fsInfoSectorNumber) { /* FAT32 only */
        self.systemInfo.fsInfoSector = [[NSMutableData alloc] initWithLength:bytesPerSector];

        NSError *err = [Utilities syncMetaReadFromDevice:self.resource
                                                    into:self.systemInfo.fsInfoSector.mutableBytes
                                              startingAt:bytesPerSector * self.systemInfo.fsInfoSectorNumber
                                                  length:bytesPerSector];
        if (err) {
            os_log_error(fskit_std_log(), "%s: Failed to read FS Info sector, error %@, ignoring", __func__, err);
            self.systemInfo.fsInfoSectorNumber = 0;
        }

        struct fsinfo* fsinfo = (struct fsinfo*)(self.systemInfo.fsInfoSector.mutableBytes);
        if (!memcmp(fsinfo->fsisig1, "RRaA", 4) &&
            !memcmp(fsinfo->fsisig2, "rrAa", 4) &&
            !memcmp(fsinfo->fsisig3, "\0\0\125\252", 4)) {
            /* If the sector has a valid signature, use the values */
            self.systemInfo.freeClusters = getuint32(fsinfo->fsinfree);
            self.systemInfo.firstFreeCluster = getuint32(fsinfo->fsinxtfree);

            /*
             * If the free clusters count is bigger than total number of clusters,
             * ignore it.
             */
            if (self.systemInfo.freeClusters > self.systemInfo.maxValidCluster - 1) {
                self.systemInfo.fsInfoSectorNumber = 0;
            }
        } else {
            os_log_error(fskit_std_log(), "%s: FS Info sector has an invalid signature", __func__);
            self.systemInfo.fsInfoSectorNumber = 0;
        }
    }

    /* Some more sanity checks per FAT type */
    if (self.systemInfo.type == FAT32) {
        self.systemInfo.rootFirstCluster = getuint32(b710->bpbRootClust);
        if (![self.systemInfo isClusterValid:self.systemInfo.rootFirstCluster]) {
            os_log_error(fskit_std_log(), "%s: FAT32 root starting cluster (%u) is out of range ([%u, %llu]",
                          __func__, self.systemInfo.rootFirstCluster, FIRST_VALID_CLUSTER,
                          self.systemInfo.maxValidCluster);
        }
        if (rootEntryCount) {
            os_log_error(fskit_std_log(), "%s: FAT32 has non-zero root directory entry count", __func__);
            return EINVAL;
        }
        if (getuint16(b710->bpbFSVers) != 0) {
            os_log_error(fskit_std_log(), "%s: FAT32 has non-zero version", __func__);
            return EINVAL;
        }
        if (getuint16(b50->bpbSectors) != 0) {
            os_log_error(fskit_std_log(), "%s: FAT32 has non-zero 16b total sectors", __func__);
            return EINVAL;
        }
        if (getuint16(b50->bpbFATsecs) != 0) {
            os_log_error(fskit_std_log(), "%s: FAT32 has non-zero 16b FAT sectors", __func__);
            return EINVAL;
        }
    } else {
        if (rootEntryCount == 0) {
            os_log_error(fskit_std_log(), "%s: FAT%d has zero length root directory", __func__,
                         self.systemInfo.type == FAT12 ? 12 : 16);
            return EINVAL;
        }
        if (totalSectors < 0x10000 && getuint16(b50->bpbSectors) == 0) {
            os_log_debug(fskit_std_log(), "%s: FAT%d total sectors (%u) fit in 16b but stored in 32b",
                         __func__, self.systemInfo.type == 12 ? 12 : 16, totalSectors);
        }
        if (getuint16(b50->bpbFATsecs) == 0) {
            os_log_debug(fskit_std_log(), "%s: FAT%d has 32b FAT sectors", __func__,
                         self.systemInfo.type == 12 ? 12 : 16);
        }
    }

    return 0;
}

-(MsdosDirItem *)createRootDirItem
{
    MsdosDirItem *rootItem = [[MsdosDirItem alloc] initInVolume:self
                                                          inDir:nil
                                                     startingAt:self.systemInfo.rootFirstCluster
                                                       withData:nil
                                                        andName:[[NSString alloc]initWithUTF8String:""]
                                                         isRoot:true];
    if (rootItem) {
        [rootItem iterateFromOffset:0
                            options:0
                       replyHandler:^iterateDirStatus(NSError * _Nonnull iterateError,
                                                      dirEntryType result,
                                                      uint64_t dirEntryOffset,
                                                      struct unistr255 * _Nullable name,
                                                      DirEntryData * _Nullable dirEntryData) {
            if (iterateError) {
                os_log_error(fskit_std_log(), "%s: Couldn't iterate root dir. Error = %@.", __func__, iterateError);
            }
            if (result == FATDirEntryVolName) {
                rootItem.entryData = dirEntryData;
            }
            /* The volume-name entry should be the first one,
             so there's no point continue searching for it.*/
            return iterateDirStop;
        }];
    }
    return rootItem;
}

/* FSVolumePathConfOperations bits */
- (int32_t)PC_CASE_PRESERVING {
	return 1;
}

- (int32_t)PC_CASE_SENSITIVE {
	return 0;
}

- (BOOL)isChownRestricted {
	return 0;
}

- (int32_t)maxFileSizeInBits {
	return 33;
}

- (int32_t)maxLinkCount {
	return 1;
}

- (int32_t)maxNameLength {
	return WIN_MAXLEN;
}

- (BOOL)islongNameTruncated {
	return 0;
}

/* FSVolumeXattrOperations */

// There will only ever be one or zero xattrs on the root item,
// so just use a static array.
static NSArray *rootItemXattrs = @ [ @MSDOSFS_XATTR_VOLUME_ID_NAME ];

-(NSArray<FSFileName *> *)supportedXattrNamesForItem:(FSItem *)item
{
    DirItem *theDirItem = [DirItem dynamicCast:item];

    if (theDirItem && theDirItem.isRoot) {
        return rootItemXattrs;
    } else {
        return nil;
    }
}

// N.B. for string constants, the compiler will const-fold strlen().
#define name_matches(name, match)                          \
        ((name).data.length == strlen(match) &&                 \
         memcmp((name).data.bytes, (match), strlen(match)) == 0)

- (void)xattrNamed:(FSFileName *)name
            ofItem:(FSItem *)item
      replyHandler:(void (^)(NSData * _Nullable value,
                             NSError * _Nullable error))reply
{
    DirItem *theDirItem = [DirItem dynamicCast:item];

    if (theDirItem && theDirItem.isRoot) {
        if (name_matches(name, MSDOSFS_XATTR_VOLUME_ID_NAME)) {
            if (!self.systemInfo.serialNumberExists) {
                return reply(nil, fs_errorForPOSIXError(ENOATTR));
            }
            NSMutableData *data = [NSMutableData dataWithLength:4];
            uint8_t *bytes = data.mutableBytes;
            uint32_t sn = self.systemInfo.serialNumber;
            bytes[0] =  sn        & 0xff;
            bytes[1] = (sn >>  8) & 0xff;
            bytes[2] = (sn >> 16) & 0xff;
            bytes[3] = (sn >> 24) & 0xff;
            return reply(data, nil);
        }
    }
    return reply(nil, fs_errorForPOSIXError(ENOTSUP));
}

- (void)setXattrNamed:(FSFileName *)name
               toData:(NSData * _Nullable)value
               onItem:(FSItem *)item
               policy:(FSSetXattrPolicy)policy
         replyHandler:(void (^)(NSError * _Nullable error))reply
{
    DirItem *theDirItem = [DirItem dynamicCast:item];

    if (theDirItem && theDirItem.isRoot) {
        if (name_matches(name, MSDOSFS_XATTR_VOLUME_ID_NAME)) {
            return reply(fs_errorForPOSIXError(EPERM));
        }
    }
    return reply(fs_errorForPOSIXError(ENOTSUP));
}

#undef name_matches

- (void)listXattrsOfItem:(FSItem *)item
            replyHandler:(void (^)(NSArray <FSFileName *> * _Nullable value,
                                   NSError * _Nullable error))reply
{
    DirItem *theDirItem = [DirItem dynamicCast:item];

    if (theDirItem && theDirItem.isRoot && self.systemInfo.serialNumberExists) {
        return reply(@[[FSFileName nameWithString:@MSDOSFS_XATTR_VOLUME_ID_NAME]], nil);
    }
    return reply(nil, fs_errorForPOSIXError(ENOTSUP));
}

- (void)FatMount:(FSTaskParameters *)options
    replyHandler:(void (^)(FSItem * _Nullable, NSError * _Nullable))reply
{
    unsigned char *uuidConvArray = NULL;
    struct bootsector33 *bs33 = NULL;
    NSMutableData *bootSector = nil;
    __block NSError *nsError = nil;
    bool isReadOnly = NO;
    uint16_t bps = 0;
    uint8_t spc = 0;
    int error = 0;

    uuidConvArray = (unsigned char*)calloc(1, 40);
    if (!uuidConvArray) {
        return reply(nil, fs_errorForPOSIXError(ENOMEM));
    }

    for (NSString *opt in options) {
        if ([opt containsString:@"rdonly"]) {
            isReadOnly = YES;
        }
    }

    [Utilities setGMTDiffOffset];

    self.systemInfo = [[FileSystemInfo alloc] initWithBlockDevice:[self resource]];
    self.systemInfo.fsTypeName = @"msdos";

    /* Scan the boot sector */
    error = [self ScanBootSector];
    if (error) {
        return reply(nil, fs_errorForPOSIXError(error));
    }

    self.fsOps = [[FSOperations alloc] initWithType:self.systemInfo.type];

    self.fatManager = [[FATManager alloc] initWithDevice:self.resource
                                                    info:self.systemInfo
                                                     ops:self.fsOps
                                              usingCache:false];

    if (!self.fatManager) {
        os_log_error(fskit_std_log(), "%s: FATManager failed to init", __FUNCTION__);
        return reply(nil, fs_errorForPOSIXError(EIO));
    }

    /* Check dirty bit if we have one, FAT12 doesn't */
    if (self.systemInfo.type != FAT12) {
        /* Get FAT entry for cluster #1 and check if we had a clean shut down */
        [self.fatManager getDirtyBitValue:^(NSError * _Nullable error, dirtyBitValue value) {
            if (error) {
                os_log_error(fskit_std_log(), "%s: Failed to read dirty bit value, error: %@",
                             __FUNCTION__, error);
                nsError = error;
            } else {
                if (value == dirtyBitDirty) {
                    /* We can mount a dirty read-only device */
                    os_log_error(fskit_std_log(), "%s: Device is dirty, %s",
                                 __FUNCTION__, !isReadOnly ? "Fail" : "Continue");
                    if (!isReadOnly) {
                        nsError = fs_errorForPOSIXError(EINVAL);
                    }
                }
            }
        }];
    }

    if (nsError) {
        return reply(nil, nsError);
    }

    /* Compute the size in bytes of the volume root directory */
    if (self.systemInfo.type == FAT32) {
        [self.fatManager getContigClusterChainLengthStartingAt:self.systemInfo.rootFirstCluster
                                                  replyHandler:^(NSError * _Nullable error,
                                                                 uint32_t numOfContigClusters,
                                                                 uint32_t nextCluster) {
            if (error) {
                nsError = error;
            } else {
                self.systemInfo.rootDirSize = numOfContigClusters * self.systemInfo.bytesPerCluster;
            }
        }];
    }

    if (nsError) {
        return reply(nil, nsError);
    }

    /* create root record */
    self.rootItem = [self createRootDirItem];
    if (self.rootItem == nil) {
        os_log_fault(fskit_std_log(), "%s: Failed to create root item", __FUNCTION__);
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    /* If FAT32, get volume name from direntry */
    /* Read boot sector */
    bootSector = [[NSMutableData alloc] initWithLength:self.systemInfo.dirBlockSize];

    nsError = [Utilities syncReadFromDevice:self.resource
                                       into:bootSector.mutableBytes
                                 startingAt:0
                                     length:[self.systemInfo dirBlockSize]];
    if (nsError) {
        return reply(nil, nsError);
    }

    bs33 = &(((union bootsector*)bootSector.bytes)->bs33);
    bps = getuint16(((struct byte_bpb33*)bs33->bsBPB)->bpbBytesPerSec);
    spc = ((struct byte_bpb33*)bs33->bsBPB)->bpbSecPerClust;
    self.systemInfo.volumeLabel = [Utilities getVolumeName:self.resource
                                                       bps:bps
                                                       spc:spc
                                                bootsector:bootSector.mutableBytes
                                                     flags:LABEL_FROM_DIRENTRY | LABEL_FROM_BOOTSECT];
    if ((self.systemInfo.volumeLabel != nil) &&
        ([self.systemInfo.volumeLabel UTF8String] != NULL) &&
        [self.systemInfo.volumeLabel UTF8String][0] != '\0') {
        self.systemInfo.volumeLabelExists = true;
    }
    [Utilities enableMetaRW];
    return reply(self.rootItem, nil);
}

-(MsdosDirItem *)createDirItemWithParent:(FATItem * _Nullable)parentDir
							firstCluster:(uint32_t)firstCluster
							dirEntryData:(DirEntryData * _Nullable)dirEntryData
									name:(nonnull NSString *)name
								  isRoot:(bool)isRoot;
{
	return [[MsdosDirItem alloc] initInVolume:self
										inDir:parentDir
								   startingAt:firstCluster
									 withData:dirEntryData
									  andName:name
									   isRoot:isRoot];
}

-(MsdosFileItem *)createFileItemWithParent:(FATItem * _Nullable)parentDir
							  firstCluster:(uint32_t)firstCluster
							  dirEntryData:(DirEntryData * _Nullable)dirEntryData
									  name:(nonnull NSString *)name
{
	return [[MsdosFileItem alloc] initInVolume:self
										 inDir:parentDir
									startingAt:firstCluster
									  withData:dirEntryData
									   andName:name];
}

-(SymLinkItem *)createSymlinkItemWithParent:(FATItem * _Nullable)parentDir
							   firstCluster:(uint32_t)firstCluster
							   dirEntryData:(DirEntryData * _Nullable)dirEntryData
									   name:(nonnull NSString *)name
                              symlinkLength:(uint16_t)length
{
    SymLinkItem *item = [[SymLinkItem alloc] initInVolume:self
                                                    inDir:parentDir
                                               startingAt:firstCluster
                                                 withData:dirEntryData
                                                  andName:name
                                                   isRoot:false];
    
    if (item) {
        item.symlinkLength = length;
    }
    
    return item;
}

-(void)calcNumOfDirEntriesForName:(struct unistr255)unistrName
                     replyHandler:(void (^)(NSError * _Nullable error, uint32_t numberOfEntries))reply
{
	// Not in use, but we must pass them to msdosfs_unicode_to_dos_name:
	uint8_t lowerCaseFlags;
	uint8_t shortName[SHORT_NAME_LEN];

	int shortNameKind = msdosfs_unicode_to_dos_name(unistrName.chars, unistrName.length, shortName, &lowerCaseFlags);
	switch (shortNameKind) {
		case 0:
			/*
			 * The name is syntactically invalid.  Normally, we'd return EINVAL,
			 * but ENAMETOOLONG makes it clear that the name is the problem (and
			 * allows Carbon to return a more meaningful error).
			 */
			os_log_error(fskit_std_log(), "%s: Short name type invalid.", __func__);
			return reply(fs_errorForPOSIXError(ENAMETOOLONG), 0);
		case 1:
			// The name is already a short, DOS name, so no long name entries needed.
			return reply(nil, 1);
		case 2:
		case 3:
			// The name needs long name entries.  The +1 is for the short name entry.
			return reply(nil, msdosfs_winSlotCnt(unistrName.chars, (int)unistrName.length) + 1);
		default:
			return reply(fs_errorForPOSIXError(EINVAL), 0);
	}
}

-(void)nameToUnistr:(NSString *)name
			  flags:(uint32_t)flags
       replyHandler:(void (^)(NSError * _Nullable, struct unistr255))reply {
	struct unistr255 unistrName = {0};
    
	int error = CONV_UTF8ToUnistr255((uint8_t *)name.UTF8String, strlen(name.UTF8String), &unistrName, flags);
    return reply(error ? fs_errorForPOSIXError(error) : nil, unistrName);
}

-(NSError *)verifyFileSize:(uint64_t)fileSize
{
	if (fileSize > MAX_DOS_FILESIZE) {
		return fs_errorForPOSIXError(EFBIG);
	} else {
		return nil;
	}
}

-(bool)isOffsetInMetadataZone:(uint64_t)offset
{
    return offset < self.systemInfo.metaDataZoneSize;
}

/** For FAT32, if FSInfo sector is available, check if its freeClusters and firstFreeClusters values need to be updated. If so, update the sector and write it to disk. */
-(NSError *)sync
{
    uint32_t fsinfoNextFreeCluster = 0;
    uint32_t fsinfoFreeClusters = 0;
    struct fsinfo *fsInfo = NULL;
    NSError *err = nil;

    /* Applies to FAT32 FSs where we successfully read a valid FSInfo sector */
    if (self.systemInfo.fsInfoSector == nil || self.systemInfo.fsInfoSectorNumber == 0) {
        return err;
    }

    /* Compare the values on disk to the values we have */
    fsInfo = (struct fsinfo *)self.systemInfo.fsInfoSector.mutableBytes;
    fsinfoFreeClusters = getuint32(fsInfo->fsinfree);
    fsinfoNextFreeCluster = getuint32(fsInfo->fsinxtfree);

    if ((self.systemInfo.firstFreeCluster != fsinfoNextFreeCluster) ||
        (self.systemInfo.freeClusters != fsinfoFreeClusters)) {
        putuint32(fsInfo->fsinfree, (uint32_t)self.systemInfo.freeClusters);
        putuint32(fsInfo->fsinxtfree, self.systemInfo.firstFreeCluster);
        err = [Utilities metaWriteToDevice:self.resource
                                      from:self.systemInfo.fsInfoSector.mutableBytes
                                startingAt:self.systemInfo.fsInfoSectorNumber * self.systemInfo.bytesPerSector
                                    length:self.systemInfo.bytesPerSector];
        if (err) {
            os_log_error(fskit_std_log(), "%s: Failed to update FSInfo sector, error %@", __func__, err);
        }
    }

    return err;
}

-(void)setVolumeLabel:(NSData *)name
              rootDir:(DirItem *)rootItem
         replyHandler:(void (^)(FSFileName * _Nullable newVolumeName,
                                NSError * _Nullable error))reply
{
    int8_t fromLabel[SHORT_NAME_LEN];
    int8_t toLabel[SHORT_NAME_LEN];
    NSError *nsErr;

    int err = CONV_LabelUTF8ToUTF16LocalEncoding(name.bytes, toLabel);

    if (err) {
        return reply(nil, fs_errorForPOSIXError(err));
    }

    [self.fatManager setDirtyBitValue:dirtyBitDirty
                         replyHandler:^(NSError * _Nullable fatError) {
        if (fatError) {
            /* Log the error, keep going */
            os_log_error(fskit_std_log(), "%s: Couldn't set the dirty bit. Error = %@.", __FUNCTION__, fatError);
        }
    }];

    nsErr = [self updateLabelInBootSector:fromLabel
                                   toName:toLabel];

    if (nsErr) {
        return reply(nil, nsErr);
    }

    /*
     * Update label in root directory, if any. For now, don't
     * create one if it doesn't exist (in case devices like
     * cameras don't understand them).
     */
    if (rootItem.entryData)
    {
        [rootItem.entryData setName:(uint8_t*)toLabel];
        nsErr = [rootItem flushDirEntryData];

        if (nsErr)
        {
            /*
             * we failed to update the root dir entry but the boot sector
             * already updated. The volume name will be taken from the dir
             * entry when it exist so revert the boot sector change to avoid
             * names missmatch between the two locations.
             */
            os_log_error(fskit_std_log(), "%s: revert boot sector change", __FUNCTION__);
            [self updateLabelInBootSector:toLabel
                                   toName:fromLabel];
            return reply(nil, nsErr);
        }
    }

    /* Return the name we got, after some conversions. */
    FSFileName *newVolumeName = [Utilities getVolumeLabelFromBootSector:toLabel];
    return reply(newVolumeName, nsErr);
}

-(NSError *)updateLabelInBootSector:(int8_t[SHORT_NAME_LEN])fromShortNameLabel
                             toName:(int8_t[SHORT_NAME_LEN])toShortNameLabel
{
    NSMutableData *readBuffer = [[NSMutableData alloc] initWithLength:self.systemInfo.dirBlockSize];
    uint32_t bytesPerSector = self.systemInfo.bytesPerSector;

    /* read the boot sector from the device */
    NSError *err = [Utilities syncReadFromDevice:[self resource]
                                                into:readBuffer.mutableBytes
                                          startingAt:0
                                              length:bytesPerSector];

    if (err)
    {
        return err;
    }

    union bootsector *boot = readBuffer.mutableBytes;

    if (boot->bs50.bsJump[0] != 0xE9 &&
        boot->bs50.bsJump[0] != 0xEB) {
        os_log_error(fskit_std_log(), "Invalid jump signature (0x%02X)", boot->bs50.bsJump[0]);
        return fs_errorForPOSIXError(EINVAL);
    }
    if ((boot->bs50.bsBootSectSig0 != BOOTSIG0) ||
        (boot->bs50.bsBootSectSig1 != BOOTSIG1)) {
        /*
         * Not returning an error here as some volumes might have an unexpected
         * signature but are mounted on Windows systems and using our msdos kext.
         */
        os_log_error(fskit_std_log(), "Invalid boot signature (0x%02X 0x%02X)",
                     boot->bs50.bsBootSectSig0, boot->bs50.bsBootSectSig1);
    }

    struct extboot *extboot;

    if (self.systemInfo.type == FAT32) {
        extboot = (struct extboot *)boot->bs710.bsExt;
    } else {
        extboot = (struct extboot *)boot->bs50.bsExt;
    }

    if (extboot->exBootSignature == EXBOOTSIG) // Make sure volume has ext boot
    {
        // Get the current label
        memcpy(fromShortNameLabel, extboot->exVolumeLabel, SHORT_NAME_LEN);
        // Update with the new label
        memcpy(extboot->exVolumeLabel, toShortNameLabel, SHORT_NAME_LEN);
    }
    
    err = [Utilities metaWriteToDevice:[self resource]
                                  from:(void*)readBuffer.bytes
                            startingAt:0
                                length:bytesPerSector];
    return err;
}
@end


@implementation FileSystemInfo

-(instancetype)initWithBlockDevice:(FSBlockDeviceResource  * _Nonnull)device
{
    self = [super init];
    if (self) {
        _dirBlockSize = [device blockSize];
        _fsInfoSector = nil;
    }
    return self;
}

-(uint64_t)offsetForCluster:(uint64_t)cluster
{
    return (cluster - FIRST_VALID_CLUSTER) * self.bytesPerCluster + self.firstClusterOffset * self.bytesPerSector;
}

-(uint64_t)offsetForDirBlock:(uint64_t)dirBlock
{
    return (uint64_t)(dirBlock - self.firstDirBlockNum) * self.dirBlockSize + self.firstClusterOffset * self.bytesPerSector;
}

-(bool)isClusterValid:(uint32_t)cluster
{
    return ((cluster >= FIRST_VALID_CLUSTER) && (cluster <= self.maxValidCluster));
}

-(bool)isFAT12Or16
{
    return (self.type == FAT12 || self.type == FAT16);
}

-(uint64_t)rootDirBytes
{
    return [self isFAT12Or16] ? ((uint64_t)self.rootDirSize * (uint64_t)self.bytesPerSector) : 0;
}

-(uint64_t)fileSizeInBits
{
    return 33;
}

@end


NS_ASSUME_NONNULL_END
