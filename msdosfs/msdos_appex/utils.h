/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef utils_h
#define utils_h

#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>
#import <FSKit/FSKitTypes.h>
#import <FSKit/FSKit.h>
#import "bootsect.h"
#import "direntry.h"
#import "Conv.h"
#import "bpb.h"

#include <pwd.h>

NS_ASSUME_NONNULL_BEGIN

/*
 * The maximum file size on FAT is 4GB-1, which is the largest value that fits
 * in an unsigned 32-bit integer.
 */
#define MAX_DOS_FILESIZE 0xffffffff

#define LABEL_LENGTH 11 /* Maximal volume label length */

#define MAX_DOS_BLOCKSIZE (4096)
#define FAT_BLOCKSIZE (8192U)
#define FAT_BLOCKMASK (FAT_BLOCKSIZE - 1)

#define FREE_CLUSTER (0)
#define FIRST_VALID_CLUSTER (2) /* 2 is the minimum valid cluster number */
#define RESERVED_CLUSTERS_START (0xFFFFFFF6) /* start of reserved clusters */

#define MSDOS_MAX_DIR_BLOCK_SIZE_MACOS (4096U)
#define RESERVED_CLUSTER_RANGE (0xFFFFFFF6)
#define MASK_12BIT (0x00000FFF)
#define MASK_16BIT (0x0000FFFF)
#define MASK_32BIT (0x0FFFFFFF)

#define FAT_DIRTY_BIT_CLUSTER   1
#define FAT_16_DIRTY_BIT_IDX    15
#define FAT_32_DIRTY_BIT_IDX    27


#if (BYTE_ORDER == LITTLE_ENDIAN)             /* && defined(UNALIGNED_ACCESS) */
#define    getuint16(x)    *((u_int16_t *)(x))
#define    getuint32(x)    *((u_int32_t *)(x))
#define    putuint16(p, v)    (*((u_int16_t *)(p)) = (v))
#define    putuint32(p, v)    (*((u_int32_t *)(p)) = (v))
#else
#define getuint16(x)    (((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8))
#define getuint32(x)    (((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8) \
             + (((u_int8_t *)(x))[2] << 16)    \
             + (((u_int8_t *)(x))[3] << 24))
#define putuint16(p, v)    (((u_int8_t *)(p))[0] = (v) & 0xFF,    \
             ((u_int8_t *)(p))[1] = ((v) >> 8) & 0xFF)
#define putuint32(p, v)    (((u_int8_t *)(p))[0] = (v) & 0xFF,    \
             ((u_int8_t *)(p))[1] = ((v) >> 8) & 0xFF, \
             ((u_int8_t *)(p))[2] = ((v) >> 16) & 0xFF, \
             ((u_int8_t *)(p))[3] = ((v) >> 24) & 0xFF)
#endif

#undef ROUND_DOWN
#define ROUND_DOWN(_x, _m)    (((_x) / (_m)) * (_m))

#undef ROUND_UP
#define ROUND_UP(_x, _m)      ROUND_DOWN((_x) + (_m) - 1, (_m))


typedef NS_ENUM(uint32_t, parseCharacterResult) {
	parseCharacterResultError = 0, 	/* unexpected character */
	parseCharacterResultEnd, 		/* this is the last character in name */
	parseCharacterResultContinue 	/* continue iterating the name */
};

typedef NS_ENUM(uint8_t, dirtyBitValue) {
    dirtyBitClean = 0,
    dirtyBitDirty = 1,
    dirtyBitUnknown = 2
};

typedef NS_ENUM(uint8_t, volumeLabelSrc) {
    LABEL_FROM_DIRENTRY = 1,
    LABEL_FROM_BOOTSECT = 2
};


@interface Utilities : NSObject

+(NSString*)getVolumeName:(FSBlockDeviceResource *)device
                       bps:(uint16_t)bps
                       spc:(uint8_t)spc
                bootsector:(union bootsector *)bootSector
                    flags:(uint8_t)flags;

+(BOOL)isLabelLegal:(char *)label;

+(NSUUID*)generateVolumeUuid:(union bootsector * _Nonnull)bootSector
                        uuid:(unsigned char *)uuid;

+(FSFileName *)getVolumeLabelFromBootSector:(int8_t*)volumeLabel;

+(CFStringEncoding)getDefaultDOSEncoding;

+(parseCharacterResult)parseCharacterOfLongNameEntry:(struct winentry *)longNameEntry
                                      charIdxInEntry:(uint32_t)idxInEntry
                                       charIdxInName:(uint32_t)idxInName
                                          unistrName:(struct unistr255 *)name
                               isFirstLongEntryInSet:(bool)isFirst;

+(bool)isDotOrDotDot:(char *)name
              length:(size_t)length;

+(NSError * _Nullable)syncReadFromDevice:(FSBlockDeviceResource *)device
                                   into:(void *)buffer
                             startingAt:(off_t)offset
                                 length:(size_t)nbyte;

+(NSError * _Nullable)syncMetaReadFromDevice:(FSBlockDeviceResource *)device
                                        into:(void *)buffer
                                  startingAt:(off_t)offset
                                      length:(size_t)nbyte;

+(NSError * _Nullable)metaWriteToDevice:(FSBlockDeviceResource *)device
                                   from:(void *)buffer
                             startingAt:(off_t)offset
                                 length:(size_t)nbyte;

+(NSError * _Nullable)syncMetaClearToDevice:(FSBlockDeviceResource *)device
							  rangesToClear:(NSArray<FSMetadataBlockRange *> *)rangesToClear;

+(NSError * _Nullable)syncMetaPurgeToDevice:(FSBlockDeviceResource *)device
                              rangesToPurge:(NSArray<FSMetadataBlockRange *> *)rangesToPurge;

+(NSData *)getMD5Digest:(uint32_t)length
                forData:(const char *)data
                 length:(uint32_t)DataLeng;

+(bool)containsReadOnlyAttributes:(nonnull FSItemSetAttributesRequest *)attributes;

+(void)enableMetaRW;

+(void)setGMTDiffOffset;

@end

NS_ASSUME_NONNULL_END

#endif /* utils_h */
