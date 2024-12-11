/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <sys/stat.h>

#import "FATManager.h"
#import "msdosVolume.h"
#import "msdosItem.h"
#import "DirBlock.h"
#import "FATItem.h"
#import "utils.h"

#define MAX_AMOUNT_OF_SHORT_GENERATION_NUM (1000000)


@implementation MsdosDirEntryData

-(instancetype)initWithData:(NSData *)data;
{
    self = [super init];
    if (self) {
        self.data = [NSMutableData dataWithData:data];
    }
    return self;
}

-(instancetype)init
{
    self = [super init];
    if (self) {
        self.data = [NSMutableData dataWithLength:sizeof(struct dosdirentry)];
    }
    return self;
}

-(uint64_t)calcFirstEntryOffsetInVolume:(FileSystemInfo *)systemInfo
{
    uint64_t offset = (self.dosDirEntryDirBlockNum * systemInfo.dirBlockSize) + self.dosDirEntryOffsetInDirBlock;
    return offset;
}

-(FSItemType)type
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    if (dosDirEntry->deAttributes & ATTR_DIRECTORY) {
        return FSItemTypeDirectory;
    } else {
        // TODO: Need to check if this is a symlink or regular file
        return FSItemTypeFile;
    }
}

-(uint32_t)bsdFlags
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    uint32_t fileBSDFlags = 0;
    uint8_t deAttributes = dosDirEntry->deAttributes;

    if ((deAttributes & (ATTR_ARCHIVE | ATTR_DIRECTORY)) == 0)    // DOS: flag set means "needs to be archived"
        fileBSDFlags |= SF_ARCHIVED;                // BSD: flag set means "has been archived"
    if ((deAttributes & (ATTR_READONLY | ATTR_DIRECTORY)) == ATTR_READONLY)
        fileBSDFlags |= UF_IMMUTABLE;                // DOS read-only becomes BSD user immutable
    if (deAttributes & ATTR_HIDDEN)
        fileBSDFlags |= UF_HIDDEN;
    
    return fileBSDFlags;
}

-(NSError *)setBsdFlags:(uint32_t)newFlags
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    bool isDir = self.type == FSItemTypeDirectory ? true : false;

    if ((newFlags & ~MSDOS_VALID_BSD_FLAGS_MASK) != 0) {
        os_log_error(fskit_std_log(), "%s: invalid BSD flags (0x%x)", __func__, newFlags);
        return fs_errorForPOSIXError(EINVAL);
    }

    if (newFlags & UF_HIDDEN) {
        dosDirEntry->deAttributes |= ATTR_HIDDEN;
    } else {
        dosDirEntry->deAttributes &= ~ATTR_HIDDEN;
    }

    if (!isDir) {
        if (newFlags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
            dosDirEntry->deAttributes |= ATTR_READONLY;
        } else {
            dosDirEntry->deAttributes &= ~ATTR_READONLY;
        }

        if (newFlags & SF_ARCHIVED) {
            dosDirEntry->deAttributes &= ~ATTR_ARCHIVE;
        } else {
            dosDirEntry->deAttributes |= ATTR_ARCHIVE;
        }
    }

    return nil;
}

-(void)getAccessTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    msdosfs_dos2unixtime(getuint16(dosDirEntry->deADate), 0, 0, tp);
}

-(void)setAccessTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    msdosfs_unix2dostime(tp, (uint16_t *)dosDirEntry->deADate, NULL, NULL);
}

-(void)getModifyTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    msdosfs_dos2unixtime(getuint16(dosDirEntry->deMDate), getuint16(dosDirEntry->deMTime), 0, tp);
}

-(void)setModifyTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    msdosfs_unix2dostime(tp, (uint16_t *)dosDirEntry->deMDate, (uint16_t *)dosDirEntry->deMTime, NULL);
}

-(void)getChangeTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    msdosfs_dos2unixtime(getuint16(dosDirEntry->deMDate), getuint16(dosDirEntry->deMTime), 0, tp);
}

-(void)setChangeTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    msdosfs_unix2dostime(tp, (uint16_t *)dosDirEntry->deMDate, (uint16_t *)dosDirEntry->deMTime, NULL);
}

-(void)getBirthTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    msdosfs_dos2unixtime(getuint16(dosDirEntry->deCDate), getuint16(dosDirEntry->deCTime), 0, tp);
}

-(void)setBirthTime:(struct timespec *)tp
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    msdosfs_unix2dostime(tp, (uint16_t *)dosDirEntry->deCDate, (uint16_t *)dosDirEntry->deCTime, &dosDirEntry->deCHundredth);
}

-(uint32_t)getFirstCluster:(FileSystemInfo *)systemInfo
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
	uint32_t firstCluster = getuint16(dosDirEntry->deStartCluster);
	if (systemInfo.type == FAT32) {
		firstCluster |= (getuint16(dosDirEntry->deHighClust) << 16);
	}
	return firstCluster;
}

-(void)setFirstCluster:(uint32_t)firstCluster
		fileSystemInfo:(FileSystemInfo *)systemInfo
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
	putuint16(dosDirEntry->deStartCluster, firstCluster & 0xFFFF);
	if (systemInfo.type == FAT32) {
		putuint16(dosDirEntry->deHighClust, (firstCluster >> 16) & 0xFFFF);
	}
}

-(uint8_t *)getName
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    return dosDirEntry->deName;
}

-(void)setName:(uint8_t *)name
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    memcpy(dosDirEntry->deName, name, SHORT_NAME_LEN);
}

-(uint64_t)getSize
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.bytes;
    return (uint64_t)getuint32(dosDirEntry->deFileSize);
}

-(void)setSize:(uint64_t)size
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    putuint32(dosDirEntry->deFileSize, (uint32_t)size);
}

-(uint64_t)getValidDataLength
{
    /*
     * How far into the file user data has been successfully written.
     * This property is not written on disk for MsDOS.
     */
    return [self getSize];
}

-(void)setValidDataLength:(uint64_t)validDataLength
{
    /*
     * How far into the file user data has been successfully written.
     * This property is not written on disk for MsDOS.
     */
    [self setSize:validDataLength];
}

-(void)setLowerCaseFlags:(uint8_t)lowerCaseFlags
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    dosDirEntry->deLowerCase = lowerCaseFlags;
}

-(void)setAttrFlags:(uint8_t)attrFlags
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    dosDirEntry->deAttributes = attrFlags;
}

-(uint8_t)getAttrFlags
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;
    return dosDirEntry->deAttributes;
}

-(void)setArchiveBit
{
    struct dosdirentry *dosDirEntry = (struct dosdirentry *)self.data.mutableBytes;

    if (self.type == FSItemTypeDirectory) {
        return;
    }

    dosDirEntry->deAttributes |= ATTR_ARCHIVE;
}

@end


@interface MsdosLongNameContext : NSObject

@property uint32_t	numLongNameEntries;
@property uint32_t 	numLongNameEntriesLeft;
@property uint8_t  	checkSum;
@property bool		isValid;

-(void)fillWithFirstLongNameEntry:(struct winentry *)longNameEntry;
-(void)invalidate;

@end

@implementation MsdosLongNameContext

-(instancetype)init
{
	self = [super init];
	if (self) {
		self.numLongNameEntries = 0;
		self.numLongNameEntriesLeft = 0;
		self.checkSum = 0;
		self.isValid = false;
	}
	return self;
}

-(void)fillWithFirstLongNameEntry:(struct winentry *)longNameEntry
{
	self.numLongNameEntries = longNameEntry->weCnt & WIN_CNT;
	self.numLongNameEntriesLeft = self.numLongNameEntries;
	self.checkSum = longNameEntry->weChksum;
	self.isValid = true;
}

-(void)invalidate
{
	self.numLongNameEntries = 0;
	self.numLongNameEntriesLeft = 0;
	self.checkSum = 0;
	self.isValid = false;
}

@end

@implementation MsdosDirItem


-(instancetype)initInVolume:(msdosVolume *)volume
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
    if (!self) {
        return nil;
    }

    if (firstCluster == 0 && ((volume.systemInfo.type == FAT12) || (volume.systemInfo.type == FAT16))) {
        /* For FAT12/16, cluster chain length of root dir has to be 1. */
        self.numberOfClusters = 1;
    }

    self.maxShortNameIndex = 0;

    return self;
}


#pragma mark DirOps methods

-(uint64_t)getDirBlockSize
{
    if ([self isFat1216RootDir]) {
        return self.volume.systemInfo.rootDirSize * self.volume.systemInfo.bytesPerSector;
    }
    return self.volume.systemInfo.dirBlockSize;
}

-(uint64_t)getDirSize
{
    if ([self isFat1216RootDir]) {
        return self.volume.systemInfo.rootDirSize * self.volume.systemInfo.bytesPerSector;
    }
    return self.numberOfClusters * self.volume.systemInfo.bytesPerCluster;
}

-(uint32_t)dirEntrySize
{
    return sizeof(struct dosdirentry);
}

-(NSError *)createDotEntriesWithAttrs:(FSItemAttributes *)mkdirAttrs
{
    __block NSError *error = nil;
    [self createNewDirEntryNamed:@"."
                            type:FSItemTypeDirectory
                      attributes:mkdirAttrs
                firstDataCluster:self.firstCluster
                    replyHandler:^(NSError * _Nullable createError,
                                   uint64_t offsetInDir) {
        if (createError) {
            error = createError;
        }
    }];
    if (error) {
        os_log_error(fskit_std_log(), "%s: create '.' entry failed with error = %@.", __func__, error);
        return error;
    }

    // Get parent attributes needed for creating a '..' directory entry
    FSItemGetAttributesRequest *getAttr = [self.volume getAttrRequestForNewDirEntry];
    FSItemAttributes *parentAttrs = [self.parentDir getAttributes:getAttr];

    uint32_t firstCluster = [DirItem dynamicCast:self.parentDir].isRoot ? 0 : self.parentDir.firstCluster;
    [self createNewDirEntryNamed:@".."
                            type:FSItemTypeDirectory
                      attributes:parentAttrs
                firstDataCluster:firstCluster
                    replyHandler:^(NSError * _Nullable createError,
                                   uint64_t offsetInDir) {
        if (createError) {
            error = createError;
        }
    }];
    if (error) {
        os_log_error(fskit_std_log(), "%s: create '..' entry failed with error = %@.", __func__, error);
        return error;
    }
    return nil;
}

-(NSError *)updateModificationTimeOnCreateRemove
{
    __block NSError *error = nil;
    uint32_t sectorSize = self.volume.systemInfo.bytesPerSector;
    DirBlock* dirBlock = [[DirBlock alloc] initInDir:self];
    struct timespec parentMTime = {0};
    struct timespec currentTime = {0};
    CONV_GetCurrentTime(&currentTime);

    error = [dirBlock readRelativeDirBlockNum:0];
    if (error) {
        [dirBlock releaseBlock];
        os_log_error(fskit_std_log(), "%s: Couldn't read dir block idx 0. Error = %@.", __func__, error);
        return error;
    }
    MsdosDirEntryData *dotEntryData = [[MsdosDirEntryData alloc] initWithData:[NSData dataWithBytes:[dirBlock getBytesAtOffset:0]
                                                                                             length:sizeof(struct dosdirentry)]];
    /*
     * Can update only if the folder is not the root, by using the "." entry in the directory,
     * OR if it is the root AND there is a volume label entry to use its times.
     */
    if (!(self.isRoot) ||
        ((([dotEntryData getAttrFlags] & ATTR_WIN95_MASK) != ATTR_WIN95) && ([dotEntryData getAttrFlags] & ATTR_VOLUME))) {
        // Update only if there is a diff in the modified time.
        [dotEntryData getModifyTime:&parentMTime];
        if (parentMTime.tv_sec != currentTime.tv_sec) {
            // Update modified time in '.' entry.
            [dotEntryData setModifyTime:&currentTime];
            // TODO: should we write the whole dir block in iOS?
            /* Update dir block */
            [dirBlock setBytes:dotEntryData.data
                      atOffset:0];
            error = [dirBlock writeToDiskFromOffset:0
                                             length:sectorSize];
            if (error) {
                os_log_error(fskit_std_log(), "%s: Couldn't write dir block to disk. Error = %@.", __func__, error);
                [dirBlock releaseBlock];
                return error;
            }
        }
    }
    [dirBlock releaseBlock];
    return nil;
}

-(NSError *)updateDotDirEntryTimes:(FSItemSetAttributesRequest *)attrs
{
    DirBlock* dirBlock = [[DirBlock alloc] initInDir:self];
    MsdosDirEntryData *dotEntryData = nil;
    struct timespec timeSpec = {0};
    NSError *error = nil;

    error = [dirBlock readRelativeDirBlockNum:0];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Couldn't read dir block idx 0. Error = %@.", __func__, error);
        [dirBlock releaseBlock];
        return error;
    }

    dotEntryData = [[MsdosDirEntryData alloc] initWithData:[NSMutableData dataWithBytes:[dirBlock getBytesAtOffset:0]
                                                                                 length:sizeof(struct dosdirentry)]];

    if ([attrs isValid:FSItemAttributeModifyTime]) {
        timeSpec = attrs.modifyTime;
        [dotEntryData setModifyTime:&timeSpec];
    }

    if ([attrs isValid:FSItemAttributeChangeTime]) {
        timeSpec = attrs.changeTime;
        [dotEntryData setChangeTime:&timeSpec];
    }

    if ([attrs isValid:FSItemAttributeBirthTime]) {
        timeSpec = attrs.birthTime;
        [dotEntryData setBirthTime:&timeSpec];
    }

    /* Update dir block */
    [dirBlock setBytes:dotEntryData.data
              atOffset:0];

    /* Flush it to disk */
    error = [dirBlock writeToDisk];
    [dirBlock releaseBlock];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Failed to flush dot entry to disk. Error = %@", __func__, error);
    }

    return error;
}

-(void)iterateFromOffset:(uint64_t)startOffsetInDir
				 options:(iterateDirOptions)options
            replyHandler:(iterateDirStatus (^)(NSError *error,
											   dirEntryType result,
											   uint64_t dirEntryOffset,
                                               struct unistr255 * _Nullable name,
											   DirEntryData * _Nullable dirEntryData))reply
{
	if (startOffsetInDir >= [self getDirSize]) {
		/* In case startOffsetInDir is beyond the directory size, return EINVAL */
        reply(fs_errorForPOSIXError(EINVAL), FATDirEntryUnknown, 0, nil, nil);
        return;
	}
	DirBlock *dirBlock = [[DirBlock alloc] initInDir:self];
    uint64_t clusterSize = self.volume.systemInfo.bytesPerCluster;
    uint64_t dirBlockSize = [self getDirBlockSize];
    uint64_t startClusterIdxInDir = startOffsetInDir / clusterSize;
	__block uint64_t dirClusterIdx = 0;
	__block uint64_t currentDirOffset = startOffsetInDir;
	__block MsdosLongNameContext *longNameCtx = [[MsdosLongNameContext alloc] init];
	__block MsdosDirEntryData *dirEntryData = [[MsdosDirEntryData alloc] init];
	__block struct unistr255 unistrName = {0};
	__block iterateDirStatus status = iterateDirContinue;

    /*
     * In case of FAT12/16 root directory, we don't get the cluster info from the FAT manager,
     * as we already know where the root dir data is located.
     * Also, there's only one dir block for the FAT12/16 root dir.
     * Therefore, we should start iterating in the first cluster chain.
     */
    __block bool startIterating = [self isFat1216RootDir];
	[self.volume.fatManager iterateClusterChainOfItem:self
                                         replyHandler:^iterateClustersResult(NSError *error, uint32_t startCluster, uint32_t numOfContigClusters) {
		if (error) {
			reply(error, FATDirEntryUnknown,0, nil, nil);
			return iterateClustersStop;
		}

		if (!startIterating) {
			if ((dirClusterIdx <= startClusterIdxInDir) &&
				(dirClusterIdx + numOfContigClusters > startClusterIdxInDir)) {
				/* We've got to a cluster we're interested in.
				 adjust startCluster and start iterating */
				startCluster += startClusterIdxInDir - dirClusterIdx;
                numOfContigClusters -= startClusterIdxInDir - dirClusterIdx;
				startIterating = true;
			} else {
				dirClusterIdx += numOfContigClusters;
				return iterateClustersContinue;
			}
		}

        uint64_t startDirBlock = 0;
        uint64_t endDirBlock = 0;
        if (![self isFat1216RootDir]) {
            /* In case of FAT12/16 root directory, we're good with start = end = 0,
             * because we only have one dir block, and its number is not relevant,
             * as we read from disk according to the root dir offset. */
            uint64_t dirBlocksInCluster = clusterSize / dirBlockSize;
            startDirBlock = startCluster * dirBlocksInCluster + ((currentDirOffset % clusterSize) / dirBlockSize);
            endDirBlock = (startCluster + numOfContigClusters) * dirBlocksInCluster - 1; // assuming numOfContigClusters > 0, therefore '-1' is a valid op.
        }
		bool continueIterating = true;
		for (uint64_t dirBlockNum = startDirBlock;
			 (dirBlockNum <= endDirBlock) && continueIterating;
			 dirBlockNum++) {
			NSError *readError = [dirBlock readDirBlockNum:dirBlockNum];
			if (readError) {
				reply(readError, FATDirEntryUnknown, 0, nil, nil);
				return iterateClustersStop;
			}

            /*
             offsetInVolume = (itemOffsetInDir - accOffset) + startCluster * clusterSize;
             */
			for (uint64_t offsetInDirBlock = currentDirOffset % dirBlockSize;
				 (offsetInDirBlock < dirBlockSize) && continueIterating;
				 offsetInDirBlock += sizeof(struct dosdirentry), currentDirOffset += sizeof(struct dosdirentry)) {
				struct dosdirentry *currentDirEntry = (struct dosdirentry *)[dirBlock getBytesAtOffset:offsetInDirBlock];

                if (currentDirEntry == NULL) {
                    os_log_fault(fskit_std_log(), "%s: Got NULL dir entry from dir block", __FUNCTION__);
                    reply(fs_errorForPOSIXError(EFAULT), FATDirEntryUnknown, 0, nil, nil);
                    return iterateClustersStop;
                }
				if (currentDirEntry->deName[0] == SLOT_EMPTY) {
					[longNameCtx invalidate];
					status = reply(nil, FATDirEntryEmpty, currentDirOffset, nil, nil);
				} else if (currentDirEntry->deName[0] == SLOT_DELETED) {
					[longNameCtx invalidate];
					status = reply(nil, FATDirEntryDeleted, currentDirOffset, nil, nil);
				} else if ((currentDirEntry->deAttributes & ATTR_WIN95_MASK) != ATTR_WIN95) {
					/* We've found a short name entry or volume name entries. */
                    uint64_t dirEntryOffset = currentDirOffset;
					if (longNameCtx.isValid) {
						/* We've had long-name entries before this short-name entry. */
                        dirEntryData.numberOfDirEntries = longNameCtx.numLongNameEntries + 1; // adding the short-name entry as well.
                        dirEntryOffset -= longNameCtx.numLongNameEntries * sizeof(struct dosdirentry);
						if (longNameCtx.numLongNameEntriesLeft > 0) {
							// Make sure we don't have any long name entries left.
							// If we do, this directory is corrupted. In that case, just skip this entry.
                            os_log_error(fskit_std_log(), "%s: (offset = %llu) Reached a short-name entry while we still have long-name entries left. Skipping entry.", __func__, currentDirOffset);
							[longNameCtx invalidate];
							continue;
						}
					} else {
						/* This short-name entry is a stand-alone entry. */
                        unistrName.length = msdosfs_dos2unicodefn((u_char *)currentDirEntry->deName, unistrName.chars, currentDirEntry->deLowerCase);
						dirEntryData.numberOfDirEntries = 1;
					}
                    [dirEntryData setData:[NSMutableData dataWithBytes:currentDirEntry length:sizeof(struct dosdirentry)]];
                    dirEntryData.dosDirEntryDirBlockNum = dirBlockNum;
                    dirEntryData.dosDirEntryOffsetInDirBlock = offsetInDirBlock;
                    dirEntryData.firstEntryOffsetInDir = dirEntryOffset;
					[longNameCtx invalidate];
					dirEntryType type = (currentDirEntry->deAttributes & ATTR_VOLUME) ? FATDirEntryVolName : FATDirEntryFound;
                    status = reply(nil, type, dirEntryOffset, &unistrName, dirEntryData);
				} else {

					// if we got here, we must have found a long name entry
					struct winentry *longNameEntry = (struct winentry *)currentDirEntry;
					bool isFirstEntryInSet;
					if (longNameCtx.isValid) {
						/* We're in the middle of iterating through a long name */
						isFirstEntryInSet = false;
					} else {
						/* This is the first long name entry of this file */
						if (!(longNameEntry->weCnt & WIN_LAST)) {
							// All valid sets of long dir entries must begin with an entry having this mask.
							// In case this mask isn't set, we just skip this dir entry.
                            os_log_error(fskit_std_log(), "%s: (offset = %llu) First long-name entry doesn't have the WIN_LAST mask. Skipping entry.", __func__, currentDirOffset);
							continue;
						}
						[longNameCtx fillWithFirstLongNameEntry:(struct winentry *)currentDirEntry];
						isFirstEntryInSet = true;
						unistrName.length = longNameCtx.numLongNameEntries * WIN_CHARS; // This value will be overriden by parseCharacterOfLongNameEntry in case the last (= first in order) entry contains NULL characters.
					}

					if (longNameEntry->weChksum != longNameCtx.checkSum)
					{
						// All long name entries must have the same checksum value.
						// If it's not the case, we just skip this dir entry.
                        os_log_error(fskit_std_log(), "%s: (offset = %llu) long-name entry has an invalid checksum value. Skipping entry.", __func__, currentDirOffset);
						[longNameCtx invalidate];
						continue;
					}

					uint32_t indexInName = (longNameCtx.numLongNameEntriesLeft - 1) * WIN_CHARS;
					parseCharacterResult res = parseCharacterResultError;
					for (uint32_t charIdx = 0; charIdx < WIN_CHARS; charIdx++) {
						res = [Utilities parseCharacterOfLongNameEntry:longNameEntry
														charIdxInEntry:charIdx
														 charIdxInName:indexInName
															unistrName:&unistrName
												 isFirstLongEntryInSet:isFirstEntryInSet];
						if (res == parseCharacterResultError || res == parseCharacterResultEnd) {
							break;
						}
					}
					if (res == parseCharacterResultError) {
						/* We failed parsing the name. Continue to the next dir entry */
                        os_log_error(fskit_std_log(), "%s: (offset = %llu) Failed to parse long-name entry's characters. Skipping entry.", __func__, currentDirOffset);
						[longNameCtx invalidate];
						continue;
					}
					longNameCtx.numLongNameEntriesLeft--;
				}
				if (status == iterateDirStop) {
					continueIterating = false;
				}
			}
		}
		dirClusterIdx += numOfContigClusters;
		return (status == iterateDirContinue ? iterateClustersContinue : iterateClustersStop);
	}];
    [dirBlock releaseBlock];
}

/**
 Iterate the directory, find the first available generation number for the given shortname.
 */
-(NSError *)generateUniqueShortName:(uint8_t*)shortName
                        offsetInDir:(uint64_t)offsetInDir
{
    /*
     * We need to use a generation number so the short name will be unique within the directory.
     * So we should make sure that our short name (combined with the new generation number)
     * doesn't exist in the directory, which require us to scan the whole directory for each
     * number, until we find a unique one.
     * Therefore, to not hurt performance, we start with a generation number derived from the
     * file's direntry location within the directory, which is unlikely to collide.
     * In case it does collide, we continue incrementally looking for a unique number.
     * (we wrap-around at MAX_AMOUNT_OF_SHORT_GENERATION_NUM to try all possible numbers).
     */
    uint32_t startGenerationNum = (uint32_t)(offsetInDir / sizeof(struct dosdirentry)) % MAX_AMOUNT_OF_SHORT_GENERATION_NUM;
    __block bool isShortNameUnique = false;
    __block NSError *nsError = nil;
    uint32_t generationNum = 0;
    int genError = 0;

    for (generationNum = startGenerationNum;
         generationNum < MAX_AMOUNT_OF_SHORT_GENERATION_NUM + startGenerationNum;
         generationNum++) {
        genError = msdosfs_apply_generation_to_short_name(shortName, generationNum % MAX_AMOUNT_OF_SHORT_GENERATION_NUM);
        if (genError) {
            os_log_error(fskit_std_log(), "%s: Couldn't apply generation number (%d) to short name (%s). error = %d.",
                         __FUNCTION__, generationNum, shortName, genError);
            return fs_errorForPOSIXError(genError);
        }
        [self isShortNameUniqueInDir:shortName
                        replyHandler:^(NSError * _Nullable lookupError, bool isUnique) {
            if (lookupError) {
                os_log_error(fskit_std_log(), "%s: short name lookup failed with error = %@.", __FUNCTION__, lookupError);
                nsError = lookupError;
            } else {
                isShortNameUnique = isUnique;
            }
        }];
        if (nsError) {
            return nsError;
        } else if (isShortNameUnique) {
            /* Found a unique short name. */
            break;
        }
    }
    if (!isShortNameUnique) {
        os_log_error(fskit_std_log(), "%s: Couldn't find a unique generation number for shortname creation.", __func__);
        return fs_errorForPOSIXError(EINVAL);
    }

    if (generationNum > self.maxShortNameIndex) {
        self.maxShortNameIndex = generationNum;
    }

    return nsError;
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
    MsdosDirEntryData *dirEntryData = [[MsdosDirEntryData alloc] init];
	uint8_t lowerCaseFlags;
	uint8_t shortName[SHORT_NAME_LEN];
	memset(shortName, 0x20, SHORT_NAME_LEN);

	// Determine the number of consecutive directory entries we'll need.
	int shortNameKind = msdosfs_unicode_to_dos_name(unistrName.chars, unistrName.length, shortName, &lowerCaseFlags);

	if (shortNameKind == 3) {
		/*
		 * shortNameKind != 3 means that the short name is case-insensitively
		 * equal to the long name, so don't use a generation number.
		 */
		uint32_t generationNum = self.maxShortNameIndex;
        int genError = 0;

        if (generationNum > 0 && generationNum < MAX_AMOUNT_OF_SHORT_GENERATION_NUM) {
            genError = msdosfs_apply_generation_to_short_name(shortName, generationNum);
            if (genError) {
                os_log_error(fskit_std_log(), "%s: Couldn't apply generation number (%d) to short name (%s). error = %d.", __FUNCTION__, generationNum, shortName, genError);
                return reply(fs_errorForPOSIXError(genError), nil);
            }
            self.maxShortNameIndex++;
        } else {
            if ([self generateUniqueShortName:shortName offsetInDir:offsetInDir]) {
                os_log_error(fskit_std_log(), "%s: Couldn't find a unique generation number for shortname creation.", __FUNCTION__);
                return reply(fs_errorForPOSIXError(EINVAL), nil);
            }
        }
	}

	/* If we got here we have a unique short name. Now create the entry. */

	// Update lower case flags
    [dirEntryData setLowerCaseFlags:lowerCaseFlags];

	// Update name
    [dirEntryData setName:shortName];

    // Update given start cluster
    [dirEntryData setFirstCluster:firstCluster
                   fileSystemInfo:self.volume.systemInfo];

    // Update size & attr flags
    uint32_t size = 0;
    uint8_t attrFlags = 0;
    if (hidden) {
        /* If the file name starts with ".", make it invisible on Windows. */
        attrFlags |= ATTR_HIDDEN;
    }

	if (itemType == FSItemTypeDirectory) {
        attrFlags |= ATTR_DIRECTORY;
	} else if (itemType == FSItemTypeSymlink) {
        attrFlags |= ATTR_ARCHIVE;
        size = sizeof(struct symlink);
	} else {
		// Size has already been checked against the maximum.
        attrFlags |= ATTR_ARCHIVE;
        size = [attrs isValid:FSItemAttributeSize] ? (uint32_t)[attrs size] : 0;
	}
    [dirEntryData setSize:size];
    [dirEntryData setAttrFlags:attrFlags];

	// Update times
    struct timespec attrTimeSpec;
    struct timespec curTime;
	CONV_GetCurrentTime(&curTime);

    if ([attrs isValid:FSItemAttributeBirthTime]) {
        attrTimeSpec = attrs.birthTime;
        [dirEntryData setBirthTime:&attrTimeSpec];
	} else {
        [dirEntryData setBirthTime:&curTime];
	}

    if ([attrs isValid:FSItemAttributeModifyTime]) {
        attrTimeSpec = attrs.modifyTime;
        [dirEntryData setModifyTime:&attrTimeSpec];
    } else {
        [dirEntryData setModifyTime:&curTime];
	}

    if ([attrs isValid:FSItemAttributeAccessTime]) {
        attrTimeSpec = attrs.accessTime;
        [dirEntryData setAccessTime:&attrTimeSpec];
	} else {
        [dirEntryData setAccessTime:&curTime];
	}

	return reply(nil, dirEntryData);
}

-(NSError *)writeDirEntriesToDisk:(DirEntryData *)dirEntryData
						 atOffset:(uint64_t)startOffsetInDir
							 name:(struct unistr255)name
				  numberOfEntries:(uint32_t)numberOfEntries
{
	switch (name.length) {
		case 1:
		case 2:
			if (name.chars[0] == '.' &&
				(name.length == 1 || name.chars[1] == '.')) {
				// Don't apply the no-trailing-dot rule to "." and "..".
				// Note: We will always write it out correctly to disk
				// (because the short name entry has already been created),
				// but we don't want to screw up the hash calculation for
				// later.
				break;
			}
			// otherwise, fall-through...
		default:
			CONV_ConvertToFSM(&name);
			break;
	}

	// Need to update modification time
    // XXXta: If the only place we call writeDirEntriesToDisk is on create, we can remove this code.
	struct timespec spec;
	CONV_GetCurrentTime(&spec);
    [dirEntryData setModifyTime:&spec];

    // Calculate checksum to put in long name entries.
	uint8_t checkSum = msdosfs_winChksum([dirEntryData getName]);

    __block NSError *error = nil;
    __block uint32_t entryIdx = 0;
    [self iterateDirEntriesAtOffset:startOffsetInDir
                         numEntries:numberOfEntries
                  shouldWriteToDisk:true
                       replyHandler:^(NSError *iterateError, void *dirEntry) {
        if (iterateError) {
            os_log_error(fskit_std_log(), "%s: Couldn't iterate dir entries. Error = %@.", __func__, iterateError);
            error = iterateError;
        } else {
            if (entryIdx < numberOfEntries - 1) {
                // Insert long name entry
                struct winentry longNameEntry;
                msdosfs_unicode2winfn(name.chars, name.length, &longNameEntry, numberOfEntries - entryIdx - 1, checkSum);
                memcpy(dirEntry, &longNameEntry, sizeof(struct dosdirentry));
            } else {
                // Insert short name entry
                memcpy(dirEntry, dirEntryData.data.bytes, sizeof(struct dosdirentry));
            }
            entryIdx++;
        }
    }];
	return error;
}

-(NSError *)writeDirEntryDataToDisk:(DirEntryData *)dirEntryData
{
    NSError *error = nil;
    MsdosDirEntryData *msdosDirEntryData = [MsdosDirEntryData dynamicCast:dirEntryData];
    DirBlock *dirBlock = [[DirBlock alloc] initInDir:self];
    if (!dirBlock) {
        return fs_errorForPOSIXError(ENOMEM);
    }

    // Read dir block to memory.
    error = [dirBlock readDirBlockNum:msdosDirEntryData.dosDirEntryDirBlockNum];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Couldn't read dir block idx (%llu). Error = %@.", __func__, msdosDirEntryData.dosDirEntryDirBlockNum, error);
        [dirBlock releaseBlock];
        return error;
    }

    // Copy the updated data to dir block.
    memcpy([dirBlock getBytesAtOffset:msdosDirEntryData.dosDirEntryOffsetInDirBlock], msdosDirEntryData.data.bytes, sizeof(struct dosdirentry));

    // Write the dir block to disk
    // XXXta: TODO: do we want to do sub-block writes in macOS?
    error = [dirBlock writeToDisk];
    [dirBlock releaseBlock];
    return error;
}

-(NSError *)markDirEntriesAsDeleted:(FATItem *)forItem
{
    __block NSError *error = nil;

    [self iterateDirEntriesAtOffset:forItem.entryData.firstEntryOffsetInDir
                         numEntries:forItem.entryData.numberOfDirEntries
                  shouldWriteToDisk:true
                       replyHandler:^(NSError *iterateError, void *dirEntry) {
        if (iterateError) {
            os_log_error(fskit_std_log(), "%s: Couldn't iterate dir entries. Error = %@.", __func__, iterateError);
            error = iterateError;
        } else {
            ((struct dosdirentry *)dirEntry)->deName[0] = SLOT_DELETED;
        }
    }];

    return error;
}


-(NSError *)verifyCookieOffset:(uint32_t)offset
{
    __block NSError *err = nil;

    // in case the cookie-offset = 0 there's nothing to check.
    if (offset == 0) {
        return nil;
    }

    [self iterateDirEntriesAtOffset:offset
                         numEntries:1
                  shouldWriteToDisk:false
                       replyHandler:^(NSError * _Nullable error, void * _Nullable dirEntry) {
        
        err = error;
        if (!err) {
            struct dosdirentry *dosEntry = (struct dosdirentry *)dirEntry;
            if (dosEntry->deName[0] == SLOT_EMPTY) {
                os_log_error(fskit_std_log(), "%s: [%u] points to an empty dir entry.", __FUNCTION__, offset);
                err = fs_errorForPOSIXError(EINVAL);
            }

            /*
             * The "last" long name entry is the one we encounter first in the directory,
             * so if the cookie points to a long name entry it should be the last one.
             */
            struct winentry *longEntry = (struct winentry*)dirEntry;
            if ((dosEntry->deName[0] != SLOT_DELETED) && ((dosEntry->deAttributes & ATTR_WIN95_MASK) == ATTR_WIN95) && (!(longEntry->weCnt & WIN_LAST))) {
                os_log_error(fskit_std_log(), "%s: [%u] points to non-last long-name dir entry.", __FUNCTION__, offset);
                err = fs_errorForPOSIXError(EINVAL);
            }
        }
    }];

    return err;
}

#pragma mark Overriden FATItem methods

/*
 * MsdosDirItem override getAttributes for two reasons:
 *     1. allocSize for FAT12/16 root dir should be taken from boot sector data
 *     2. Time attributes should be taken for '.' entry
 * All other attributes should be set by FATItem implementation
 */
-(FSItemAttributes *)getAttributes:(nonnull FSItemGetAttributesRequest *)desired
{
    FSItemAttributes *attrs = [super getAttributes:desired];

    if ([desired isAttributeWanted:FSItemAttributeType]) {
        attrs.type = FSItemTypeDirectory;
    }

    if (self.isRoot) {
        /* The root dir is the only dir which currently supports limited xattrs. */
        attrs.supportsLimitedXAttrs = true;
    }

    if ([self isFat1216RootDir]) {
        if ([desired isAttributeWanted:FSItemAttributeAllocSize]) {
            attrs.allocSize = [self.volume.systemInfo rootDirBytes];
        }

        if ([desired isAttributeWanted:FSItemAttributeParentID]) {
            /* For FAT12/16 root dir, we should put parentID = fileID = FSItemIDParentOfRoot. */
            attrs.parentID = [self getFileID];
        }

        if ([desired isAttributeWanted:FSItemAttributeSize]) {
            attrs.size = [self.volume.systemInfo rootDirBytes];
        }

        bool timeWanted = ([desired isAttributeWanted:FSItemAttributeAccessTime] ||
                           [desired isAttributeWanted:FSItemAttributeModifyTime] ||
                           [desired isAttributeWanted:FSItemAttributeChangeTime] ||
                           [desired isAttributeWanted:FSItemAttributeBirthTime]);

        if (timeWanted && (self.entryData == nil)) {
            struct timespec sEpochTimespec;
            uint16_t epochTime;
            uint16_t epochDate;
            // synthesize epoch date for fat32/exfat (Jan 1 1980)
            epochTime = 0x0000; /* 00:00:00 */
            epochDate = ((0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT) | (1 << DD_DAY_SHIFT));

            msdosfs_dos2unixtime(epochDate, epochTime, 0, &sEpochTimespec);

            if ([desired isAttributeWanted:FSItemAttributeAccessTime]) {
                attrs.accessTime = sEpochTimespec;
            }

            if ([desired isAttributeWanted:FSItemAttributeModifyTime]) {
                attrs.modifyTime = sEpochTimespec;
            }

            if ([desired isAttributeWanted:FSItemAttributeChangeTime]) {
                attrs.changeTime = sEpochTimespec;
            }

            if ([desired isAttributeWanted:FSItemAttributeBirthTime]) {
                attrs.birthTime = sEpochTimespec;
            }
        }

        return attrs;
    }

    if (self.isDeleted) {
        return attrs;
    }

    // For directories take the times from '.' entry.
    DirBlock *dirBlock = [[DirBlock alloc] initInDir:self];
    [dirBlock readRelativeDirBlockNum:0];
    MsdosDirEntryData *dotDirEntryData = [[MsdosDirEntryData alloc] initWithData:[NSData dataWithBytes:[dirBlock getBytesAtOffset:0]
                                                                                                length:sizeof(struct dosdirentry)]];
    struct timespec timeSpec;

    if ([desired isAttributeWanted:FSItemAttributeAccessTime]) {
        [dotDirEntryData getAccessTime:&timeSpec];
        attrs.accessTime = timeSpec;
    }

    if ([desired isAttributeWanted:FSItemAttributeModifyTime]) {
        [dotDirEntryData getModifyTime:&timeSpec];
        attrs.modifyTime = timeSpec;
    }

    if ([desired isAttributeWanted:FSItemAttributeChangeTime]) {
        [dotDirEntryData getChangeTime:&timeSpec];
        attrs.changeTime = timeSpec;
    }

    if ([desired isAttributeWanted:FSItemAttributeBirthTime]) {
        [dotDirEntryData getBirthTime:&timeSpec];
        attrs.birthTime = timeSpec;
    }
    [dirBlock releaseBlock];
    return attrs;
}

-(uint64_t)getFileID
{
    if ([self isFat1216RootDir]) {
        /* The root dir in FAT12 and FAT16 have a special hard-coded file ID. */
        return ROOT_DIR_FILENUM;
    } else {
        return [super getFileID];
    }
}


#pragma mark Internal methods

-(uint32_t)getGenerationNumberOfName:(struct unistr255 *)name
{
    unsigned long generationNumber = 0;
    char *ptr = NULL;
    int index = name->length - 1;

    if (name == NULL || name->length < 1) {
        return 0;
    }
    /*
     * We know that short name is formatted as follows:
     * shortname~generationnumber
     * Go backwards until we find the last ~, what we have before it is the
     * generation number.
     */
    while(name->chars[index] != '~' && index > 0) {
        index--;
    }

    if (index == 0) {
        return 0;
    }

    ptr = (char*)name->chars + index;
    if (ptr != NULL) {
        generationNumber = strtol(ptr, NULL, 10);
    }

    return (uint32_t)generationNumber;
}

/**
 Whether or not the given short-name already exists in the directory.
 This method also initializes the directory's maxShortNameIndex for future uses
 as it already iterates the dir entries.
 */
-(void)isShortNameUniqueInDir:(uint8_t *)shortName
                 replyHandler:(void (^)(NSError * _Nullable error, bool isUnique))reply
{
    __block NSError *iterateDirError = nil;
    __block bool foundDirEntry = false;
    __block uint32_t maxGenNum = 0;

    [self iterateFromOffset:0
                    options:0
               replyHandler:^iterateDirStatus(NSError * _Nonnull error,
                                              dirEntryType result,
                                              uint64_t dirEntryOffset,
                                              struct unistr255 * _Nullable name,
                                              DirEntryData * _Nullable dirEntryData) {
        if (error) {
            iterateDirError = error;
            return iterateDirStop;
        }
        if (result == FATDirEntryFound) {
            if (self.maxShortNameIndex < MAX_AMOUNT_OF_SHORT_GENERATION_NUM) {
                uint32_t tempGenNum = [self getGenerationNumberOfName:name];
                if (tempGenNum > maxGenNum) {
                    maxGenNum = tempGenNum;
                }
            }
            struct dosdirentry *dosDirEntry = (struct dosdirentry *)dirEntryData.data.bytes;
            if(!memcmp(shortName, dosDirEntry->deName, SHORT_NAME_LEN)) {
                foundDirEntry = true;
                if (self.maxShortNameIndex == 0) {
                    /*
                     * If we don't have a max generation number, continue
                     * iterating the directory to find it.
                     */
                    return iterateDirContinue;
                } else {
                    /*
                     * maxShortNameIndex wrapped around, we don't need to set
                     * it. Just find the first free available generation number
                     * for this short name.
                     */
                    reply(nil, false);
                    return iterateDirStop;
                }
            }
        } else if (result == FATDirEntryEmpty) {
            /* There are no dir entries after an empty one. */
            return iterateDirStop;
        }
        return iterateDirContinue;
    }];

    if (self.maxShortNameIndex == 0) {
        self.maxShortNameIndex = maxGenNum;
    }

    if (iterateDirError) {
        os_log_error(fskit_std_log(), "%s: Failed to iterate directory. Error = %@.", __func__, iterateDirError);
        reply(iterateDirError, false);
    }

    reply(nil, !foundDirEntry);
}

@end


@implementation MsdosFileItem : FileItem

-(uint64_t)maxFileSize
{
    return DOS_FILESIZE_MAX;
}

-(void)waitForWrites
{
    while (self.writeCounter > 0) {
        usleep(100);
    }
}

@end

