/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import "utils.h"
#import "Conv.h"

NS_ASSUME_NONNULL_BEGIN


@interface Utilities ()

@property bool useMetaRW;

@end

@implementation Utilities

+ (Utilities *) sharedUtilities
{
    static dispatch_once_t pred = 0;
    static Utilities *sharedObject = nil;
    dispatch_once(&pred, ^{
        sharedObject = [[self alloc] init];
        sharedObject.useMetaRW = false;
    });
    return sharedObject;
}

+(NSString*)getVolumeName:(FSBlockDeviceResource *)device
                      bps:(uint16_t)bps
                      spc:(uint8_t)spc
               bootsector:(union bootsector *)bootSector
                    flags:(uint8_t)flags
{
    struct byte_bpb710 *b710 = (struct byte_bpb710 *)bootSector->bs710.bsBPB;
    struct byte_bpb33 *b33 = (struct byte_bpb33 *)bootSector->bs33.bsBPB;
    struct byte_bpb50 *b50 = (struct byte_bpb50 *)bootSector->bs50.bsBPB;
    char diskLabel[LABEL_LENGTH] = {0};
    unsigned int rootDirSectors = 0;
    struct dosdirentry *dirp;
    NSString *volName = nil;
    BOOL finished = false;
    NSError *error = nil;
    int i = 0;

    if (flags & LABEL_FROM_DIRENTRY) {
        rootDirSectors = ((getuint16(b50->bpbRootDirEnts) * sizeof(struct dosdirentry)) + (bps-1)) / bps;
        if (rootDirSectors) {
            /* This is FAT12/16 */
            char rootdirbuf[MAX_DOS_BLOCKSIZE];
            unsigned firstRootDirSecNum;
            int j = 0;
            
            firstRootDirSecNum = getuint16(b33->bpbResSectors) + (b33->bpbFATs * getuint16(b33->bpbFATsecs));
            for (i=0; i< rootDirSectors; i++) {
                error = [self syncReadFromDevice:device
                                            into:rootdirbuf
                                      startingAt:((firstRootDirSecNum+i)*bps)
                                          length:bps];
                if (error != nil) {
                    return nil;
                }
                dirp = (struct dosdirentry *)rootdirbuf;
                for (j = 0; j < bps; j += sizeof(struct dosdirentry), dirp++) {
                    if ((dirp)->deName[0] == SLOT_EMPTY) {
                        finished = true;
                        break;
                    } else if (dirp->deName[0] == SLOT_DELETED) {
                        continue;
                    } else if (dirp->deAttributes == ATTR_WIN95) {
                        continue;
                    } else if ((dirp)->deAttributes & ATTR_VOLUME) {
                        strncpy(diskLabel, (char*)dirp->deName, LABEL_LENGTH);
                        finished = true;
                        break;
                    }
                }
                if (finished) {
                    break;
                }
            }
        } else {
            /* This is FAT32 */
            uint32_t bytesPerCluster = (uint32_t)bps * (uint32_t)spc;
            NSMutableData *rootDirBuffer = [[NSMutableData alloc] initWithLength:bytesPerCluster];
            uint32_t cluster = getuint32(b710->bpbRootClust);
            off_t readOffset;
            
            while (!finished && cluster >= FIRST_VALID_CLUSTER && cluster < RESERVED_CLUSTERS_START) {
                /* Find sector where clusters start */
                readOffset = getuint16(b710->bpbResSectors) + (b710->bpbFATs * getuint32(b710->bpbBigFATsecs));
                /* Find sector where "cluster" starts */
                readOffset += ((off_t) cluster - FIRST_VALID_CLUSTER) * (off_t) spc;
                /* Convert to byte offset */
                readOffset *= (off_t) bps;
                
                /* Read the cluster */
                error = [self syncReadFromDevice:device
                                            into:rootDirBuffer.mutableBytes
                                      startingAt:readOffset
                                          length:bytesPerCluster];
                if (error != nil) {
                    return nil;
                }
                dirp = (struct dosdirentry *)rootDirBuffer.bytes;
                
                /* iterate the directory entries looking for volume label */
                for (i = 0; i < bytesPerCluster; i += sizeof(struct dosdirentry), dirp++) {
                    if ((dirp)->deName[0] == SLOT_EMPTY) {
                        finished = true;
                        break;
                    } else if (dirp->deName[0] == SLOT_DELETED) {
                        continue;
                    } else if (dirp->deAttributes == ATTR_WIN95) {
                        continue;
                    } else if ((dirp)->deAttributes & ATTR_VOLUME) {
                        strncpy(diskLabel, (char *)dirp->deName, LABEL_LENGTH);
                        finished = true;
                        break;
                    }
                }
                if (finished) {
                    break;
                }
                
                /* Find next cluster in the chain by reading the FAT: */
                /* First FAT sector */
                readOffset = getuint16(b710->bpbResSectors);
                /* Find sector containing "cluster" entry in FAT */
                readOffset += (cluster * 4) / bps;
                /* Convert to byte offset */
                readOffset *= bps;
                /* Now read one sector of the FAT */
                error = [self syncReadFromDevice:device
                                            into:rootDirBuffer.mutableBytes
                                      startingAt:readOffset
                                          length:bps];
                if (error != nil) {
                    return nil;
                }
                
                cluster = getuint32(rootDirBuffer.bytes + ((cluster * 4) % bps));
                cluster &= 0x0FFFFFFF; /* Ignore reserved upper bits */
            }
        }
    }

    /* Get label form boot sector */
    if ((diskLabel[0] == 0) && (flags & LABEL_FROM_BOOTSECT)) {
        if (bootSector == NULL) {
            os_log_error(fskit_std_log(), "%s: bootsector wasn't given", __func__);
        }
        if (getuint16(b50->bpbRootDirEnts) == 0) {
            /* FAT32 */
            if (((struct extboot*)bootSector->bs710.bsExt)->exBootSignature == EXBOOTSIG) {
                strncpy(diskLabel, (char *)((struct extboot *)bootSector->bs710.bsExt)->exVolumeLabel, LABEL_LENGTH);
            }
        } else if (((struct extboot *)bootSector->bs50.bsExt)->exBootSignature == EXBOOTSIG) {
            strncpy(diskLabel, (char *)((struct extboot *)bootSector->bs50.bsExt)->exVolumeLabel, LABEL_LENGTH);
        }
    }

    /* Set the file system name */

    /* Convert leading 0x05 to 0xE5 for multibyte languages like Japanese */
    if (diskLabel[0] == 0x05) {
        diskLabel[0] = 0x0E5;
    }

    /* Check for illegal characters */
    if (![self isLabelLegal:diskLabel]) {
        diskLabel[0] = 0;
    }

    /* Remove trailing spaces */
    for (i = LABEL_LENGTH - 1; i >= 0; i--) {
        if (diskLabel[i] == ' ') {
            diskLabel[i] = 0;
        } else {
            break;
        }
    }

    /* Convert the label to UTF-8 */
    NSStringEncoding encoding = CFStringConvertEncodingToNSStringEncoding([self getDefaultDOSEncoding]);
    volName = [[NSString alloc] initWithBytes:diskLabel length:LABEL_LENGTH encoding:encoding];

    return volName;
}

+(BOOL)isLabelLegal:(char *)label
{
    int i = 0;
    int c = 0;

    for (i = 0, c = 0; i < LABEL_LENGTH; i++) {
        c = (u_char)label[i];
        /* First charachter can't be a blank space */
        if (c < ' ' + !i || strchr("\"*+,./:;<=>?[\\]|", c)) {
            os_log(fskit_std_log(), "%s: Illegal character: %c", __FUNCTION__, c);
            return false;
        }
    }
    return true;
}

//FIXME: Any reason not to move on to SHA-2?
+(NSUUID*)generateVolumeUuid:(union bootsector * _Nonnull)bootSector
                        uuid:(unsigned char *)uuid
{
    struct byte_bpb50 *b50 = (struct byte_bpb50 *)bootSector->bs50.bsBPB;
    struct extboot *extboot;
    NSUUID *result = nil;
    char uuid_out[40];

    if (getuint16(b50->bpbRootDirEnts) == 0){
        /* FAT32 */
        extboot = (struct extboot *)((char*)bootSector + 64);
    } else {
        /* FAT12 or FAT16 */
        extboot = (struct extboot *)((char*)bootSector + 36);
    }

    /* If there's a non-zero volume ID, convert it to UUID */
    if (extboot->exBootSignature == EXBOOTSIG &&
        (extboot->exVolumeID[0] || extboot->exVolumeID[1] ||
         extboot->exVolumeID[2] || extboot->exVolumeID[3])) {
        /* Get the total sectors as a 32-bit value */
        uint32_t total_sectors = getuint16(b50->bpbSectors);
        if (total_sectors == 0) {
            total_sectors = getuint32(b50->bpbHugeSectors);
        }
        CC_MD5_CTX c;
        uint8_t sectorsLittleEndian[4];

        UUID_DEFINE( kFSUUIDNamespaceSHA1, 0xB3, 0xE2, 0x0F, 0x39, 0xF2, 0x92, 0x11, 0xD6, 0x97, 0xA4, 0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC );

        /*
         * Normalize totalSectors to a little endian value so that this returns the
         * same UUID regardless of endianness.
         */
        putuint32(sectorsLittleEndian, total_sectors);

        /*
         * Generate an MD5 hash of our "name space", and our unique bits of data
         * (the volume ID and total sectors).
         */
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CC_MD5_Init(&c);
        CC_MD5_Update(&c, kFSUUIDNamespaceSHA1, sizeof(uuid_t));
        CC_MD5_Update(&c, extboot->exVolumeID, 4);
        CC_MD5_Update(&c, sectorsLittleEndian, sizeof(sectorsLittleEndian));
        CC_MD5_Final(uuid, &c);
    #pragma clang diagnostic pop

        /* Force the resulting UUID to be a version 3 UUID. */
        uuid[6] = (uuid[6] & 0x0F) | 0x30;
        uuid[8] = (uuid[8] & 0x3F) | 0x80;
    }
    uuid_unparse(uuid, uuid_out);
    result = [[NSUUID alloc] initWithUUIDString:[[NSString alloc] initWithUTF8String:uuid_out]];
    return result;
}

uint16_t dos2unicodeConv[32] = {
  0x20AC, 0x003f, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, /* 80-87 */
  0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003f, 0x017D, 0x003f, /* 88-8F */
  0x003f, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, /* 90-97 */
  0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003f, 0x017E, 0x0178, /* 98-9F */
};

+(FSFileName *)getVolumeLabelFromBootSector:(int8_t*)volumeLabel
{
    uint8_t tmpVolumeLabel[64] = {0};
    FSFileName *result = nil;
    unsigned char ch = 0;
    int i = 0;

    for (i = 0; i < SHORT_NAME_LEN; i++) {
        ch = volumeLabel[i];
        if (i == 0 && ch == SLOT_E5) {
            ch = 0xE5;
        }
        tmpVolumeLabel[i] = (ch < 0x80 || ch > 0x9F ? ch : dos2unicodeConv[ch - 0x80]);
    }

    /* Remove trailing spaces */
    for (i = 10; (i >= 0) && (tmpVolumeLabel[i] == ' '); --i) ; /* Do nothing */

    /* Add NULL terminator */
    tmpVolumeLabel[i+1] = '\0';
    result = [FSFileName nameWithCString:(const char*)tmpVolumeLabel];

    return result;
}

+(CFStringEncoding)getDefaultDOSEncoding
{
    NSString *tmp = [[[NSProcessInfo processInfo] environment] objectForKey:@"__CF_USER_TEXT_ENCODING"];
    tmp = [tmp substringFromIndex:tmp.length - 3]; /* Remove the user prefix, only leave the encoding */
    CFStringEncoding encoding = (unsigned int)strtol(tmp.UTF8String, NULL, 0);

    /* Convert the Mac encoding to DOS/Windows one */
    switch (encoding) {
        case kCFStringEncodingMacRoman:
            return kCFStringEncodingDOSLatin1;
        case kCFStringEncodingMacJapanese:
            return kCFStringEncodingDOSJapanese;
        case kCFStringEncodingMacChineseTrad:
            return kCFStringEncodingDOSChineseTrad;
        case kCFStringEncodingMacKorean:
            return kCFStringEncodingDOSKorean;
        case kCFStringEncodingMacArabic:
            return kCFStringEncodingDOSArabic;
        case kCFStringEncodingMacHebrew:
            return kCFStringEncodingDOSHebrew;
        case kCFStringEncodingMacGreek:
            return kCFStringEncodingDOSGreek;
        case kCFStringEncodingMacCyrillic:
        case kCFStringEncodingMacUkrainian:
            return kCFStringEncodingDOSCyrillic;
        case kCFStringEncodingMacThai:
            return kCFStringEncodingDOSThai;
        case kCFStringEncodingMacChineseSimp:
            return kCFStringEncodingDOSChineseSimplif;
        case kCFStringEncodingMacCentralEurRoman:
        case kCFStringEncodingMacCroatian:
        case kCFStringEncodingMacRomanian:
            return kCFStringEncodingDOSLatin2;
        case kCFStringEncodingMacTurkish:
            return kCFStringEncodingDOSTurkish;
        case kCFStringEncodingMacIcelandic:
            return kCFStringEncodingDOSIcelandic;
        case kCFStringEncodingMacFarsi:
            return kCFStringEncodingDOSArabic;
        default:
            return kCFStringEncodingInvalidId;
    }
}

/*
 * This is an internal helper function, extracted for unit-testing purposes.
 * Validate a uint16 character of a long name entry.
 */
+(parseCharacterResult)parseCharacterOfLongNameEntry:(struct winentry *)longNameEntry
									  charIdxInEntry:(uint32_t)idxInEntry
									   charIdxInName:(uint32_t)idxInName
										  unistrName:(struct unistr255 *)name
							   isFirstLongEntryInSet:(bool)isFirst
{
	if (idxInEntry >= WIN_CHARS) {
		/* Preventing an OOB access */
		return parseCharacterResultError;
	}

	uint8_t *currChar = (uint8_t *)longNameEntry + puLongNameOffset[idxInEntry];
	uint16_t currUnichar = currChar[0] | (currChar[1] << 8);

	if (currUnichar == 0xFFFF) {
		/*
		 * Unexpected padding with a leading \0. Let's fix this and move on,
		 * given that Windows does the same.
		 */
        currUnichar = 0;
	}
	if (currUnichar == 0) {
		/* NULL termination character */
		if (!isFirst) {
			/* We only expect to have NULL termination in the first
			 (which is actually the last) long name entry */
			return parseCharacterResultError;
		}
		name->length = idxInEntry + idxInName;
		/* Last entry of the long name, let's keep reading  (backwards) */
		return parseCharacterResultEnd;
	} else {
		if (idxInName + idxInEntry > WIN_MAXLEN) {
			return parseCharacterResultError;
		}
		name->chars[idxInName + idxInEntry] = currUnichar;
	}
	return parseCharacterResultContinue;
}

+(bool)isDotOrDotDot:(char *)name
              length:(size_t)length
{
    if (name == NULL) {
        return FALSE;
    }
    return (((strncmp(name, ".", 1) == 0) && length == 1) || ((strncmp(name, "..", 2) == 0) && length == 2));
}

+ (NSError * _Nullable)syncReadFromDevice:(FSBlockDeviceResource *)device
                                     into:(void *)buffer
                               startingAt:(off_t)offset
                                   length:(size_t)nbyte
{
    __block NSError *error = nil;

    [device synchronousReadInto:buffer
                     startingAt:offset
                         length:nbyte
                   replyHandler:^(size_t actuallyRead, NSError * _Nullable innerError) {
        if (innerError) {
            os_log_error(fskit_std_log(), "%s: Failed to read, error %@", __FUNCTION__, innerError);
            error = innerError;
        } else if (actuallyRead != nbyte) {
            os_log_error(fskit_std_log(), "%s: Expected to read %lu bytes, read %lu", __FUNCTION__, nbyte, actuallyRead);
            /*
             * Setting to EIO for now. pread's manpage lists it as a possible
             * errno value:
             * An I/O error occurred while reading from the file system.
             */
            error = fs_errorForPOSIXError(EIO);
        }
    }];
    return error;
}

+(NSError * _Nullable)syncMetaReadFromDevice:(FSBlockDeviceResource *)device
                                        into:(void *)buffer
                                  startingAt:(off_t)offset
                                      length:(size_t)nbyte
{
    __block NSError *error = nil;
    Utilities *sharedUtilities = [self sharedUtilities];
    if (sharedUtilities.useMetaRW) {
        [device synchronousMetadataReadInto:buffer
                             startingAt:offset
                                 length:nbyte
                           replyHandler:^(NSError * _Nullable innerError) {
            if (innerError) {
                error = innerError;
                os_log_error(fskit_std_log(), "%s: Failed to meta read, error %@", __FUNCTION__, error);
            }
        }];
    } else {
        error = [Utilities syncReadFromDevice:device
                                         into:buffer
                                   startingAt:offset
                                       length:nbyte];
        if (error) {
            os_log_error(fskit_std_log(), "%s: Failed to read, error %@", __FUNCTION__, error);
        }
    }

    return error;
}

+(NSError * _Nullable)metaWriteToDevice:(FSBlockDeviceResource *)device
                                   from:(void *)buffer
                             startingAt:(off_t)offset
                                 length:(size_t)nbyte
{
    __block NSError *error = nil;
#if DEBUG
    [device synchronousMetadataWriteFrom:buffer
                          startingAt:offset
                              length:nbyte
                        replyHandler:^(NSError * _Nullable innerError) {
#else
#if TARGET_OS_OSX
    [device delayedMetadataWriteFrom:buffer
                          startingAt:offset
                              length:nbyte
                        replyHandler:^(NSError * _Nullable innerError) {
#else
    [device synchronousMetadataWriteFrom:buffer
                          startingAt:offset
                              length:nbyte
                        replyHandler:^(NSError * _Nullable innerError) {
#endif
#endif
        if (innerError) {
            error = innerError;
        }
    }];

    if (error) {
        /*
         * metaRW should be enabled after mount completed so instead of checking if
         * we should use meta write on every syncMetaWriteToDevice call, we only
         * check if meta write failed, to see if the failure is expected and we
         * should a simple write.
         */
        Utilities *sharedUtilities = [self sharedUtilities];
        if (sharedUtilities.useMetaRW == false) {
            [device synchronousWriteFrom:buffer
                              startingAt:offset
                                  length:nbyte
                            replyHandler:^(size_t actuallyWrote, NSError * _Nullable innerError) {
                if (innerError) {
                    os_log_error(fskit_std_log(), "%s: Failed to write, error %@", __FUNCTION__, innerError);
                    error = innerError;
                } else if (actuallyWrote != nbyte) {
                    os_log_error(fskit_std_log(), "%s: Expected to write %lu bytes, wrote %lu", __FUNCTION__, nbyte, actuallyWrote);
                    error = fs_errorForPOSIXError(EIO);
                }
            }];
        } else {
            os_log_error(fskit_std_log(), "%s: Failed to meta write, offset %lld, length %zu, error %@",
                         __FUNCTION__, offset, nbyte, error);
        }
    }
    return error;
}

+(NSError * _Nullable)syncMetaClearToDevice:(FSBlockDeviceResource *)device
							  rangesToClear:(NSArray<FSMetadataBlockRange *> *)rangesToClear
{
    __block NSError *error = nil;

    [device synchronousMetadataClear:rangesToClear
                            wait:false // check iOS
                    replyHandler:^(NSError * _Nullable innerError) {
        if (innerError) {
            os_log_error(fskit_std_log(), "%s: Failed to meta clear, error %@", __FUNCTION__, innerError);
            error = innerError;
        }
    }];

    return error;
}

+(NSError * _Nullable)syncMetaPurgeToDevice:(FSBlockDeviceResource *)device
                              rangesToPurge:(NSArray<FSMetadataBlockRange *> *)rangesToPurge
{
    __block NSError *error = nil;

    [device synchronousMetadataPurge:rangesToPurge
                    replyHandler:^(NSError * _Nullable innerError) {
        if (innerError) {
            os_log_error(fskit_std_log(), "%s: Failed to meta purge, error %@", __FUNCTION__, innerError);
            error = innerError;
        }
    }];

    return error;
}

+(NSData *)getMD5Digest:(uint32_t)length
                forData:(const char *)data
                 length:(uint32_t)DataLength
{
    unsigned char digestRaw[length/2];
    memset(digestRaw, 0 ,sizeof(digestRaw));
    NSMutableData *digest = [NSMutableData dataWithLength:(length+1)];
    char *digestBytes = digest.mutableBytes;
    CC_MD5_CTX context;
    CC_MD5_Init(&context);
    CC_MD5_Update(&context, data, (unsigned)DataLength);
    CC_MD5_Final(digestRaw, &context);

    for (uint8_t digestoffset = 0; digestoffset < (length/2); ++digestoffset) {
        /*
         * The "3" below is for the two hex digits plus trailing '\0'.
         * Note that the trailing '\0' from byte N is overwritten
         * by the first character of byte N+1, and that digest[] has
         * a length of 33 == 2 * 16 + 1 in order to have room for a
         * trailing '\0' after the last byte's characters.
         */
        (void) snprintf(&digestBytes[digestoffset*2], 3, "%02x", digestRaw[digestoffset]);
    }
    
    return digest;
}

+(bool)containsReadOnlyAttributes:(nonnull FSItemSetAttributesRequest *)attributes
{
    if ([attributes isValid:FSItemAttributeType] || [attributes isValid:FSItemAttributeLinkCount] ||
        [attributes isValid:FSItemAttributeAllocSize] || [attributes isValid:FSItemAttributeFileID] ||
        [attributes isValid:FSItemAttributeParentID] || [attributes isValid:FSItemAttributeChangeTime]) {
        return true;
    }
    return false;
}

+(void)enableMetaRW
{
    os_log_debug(fskit_std_log(), "%s: start", __FUNCTION__);
    [Utilities sharedUtilities].useMetaRW = true;
}

extern int32_t msdos_secondsWest;    /* In msdosfs_conv.c */

+(void)setGMTDiffOffset
{
    /* Set gmtoffset */
    time_t uGMT, uRawTime = time(NULL);
    struct tm *psPTM;
    psPTM = gmtime( &uRawTime );
    // Request that mktime() looksup dst in timezone database
    psPTM->tm_isdst = -1;
    uGMT = mktime(psPTM);
    msdos_secondsWest = -((int)difftime(uRawTime, uGMT));
}

@end

NS_ASSUME_NONNULL_END
