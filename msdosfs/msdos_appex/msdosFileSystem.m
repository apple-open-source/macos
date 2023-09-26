//
//  msdosFileSystem.m
//  fsmodule
//
//  Created by Noa Osherovich on 27/07/2022.
//

#import <FSKit/FSBlockDeviceResource_private.h>
#include <sys/_types/_size_t.h>
#import <FSKit/FSResource.h>
#import "msdosFileSystem.h"
#import "bootsect.h"
#import "bpb.h"

#include  <CommonCrypto/CommonDigest.h>
#include <stdint.h>
#include <stdlib.h>
#include <pwd.h>

#include "newfs_data_types.h"
#include "lib_newfs_msdos.h"
#include "lib_fsck_msdos.h"
#include "direntry.h"
#include "format.h"
#include "dosfs.h"
#include "ext.h"

#define	CLUST_FIRST	2                                   /* 2 is the minimum valid cluster number */
#define	CLUST_RSRVD	0xfffffff6                          /* start of reserved clusters */
#define LABEL_LENGTH 11                                 /* Maximal volume label length */
#define NEWFS_LOC_TABLE "newfs_appex"
#define TASK_PROGRESS_TIMER_INTERVAL (1 * NSEC_PER_SEC) /* 1 sec interval to track a task's progress */
#define CHECK_MAX_TIME 200                              /* checkfilesys should take 200 seconds or less */

/** Simple struct to hold required resource and filesystem to call wipeFS method from FSSimpleFileSystem */
typedef struct {
    FSMessageConnection *fsMsgConn;
    FSResource* resource;
    FSSimpleFileSystem* fs;
} NewfsCtxAppex;

void startCallback(char* description, int64_t parentUnitCount, int64_t totalCount, unsigned int *completedCount, void *updater);
void endCallback(char* description, void *updater);

/** Print function to log messages from lib fsck msdos.*/
static void
fsckPrintFunction(fsck_client_ctx_t ctx, int level, const char *fmt, va_list ap)
{
    if(ctx) {
        FSMessageConnection *fsMsgConn = (__bridge FSMessageConnection *) ctx;
        if (fsMsgConn) {
            NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:ap];
            /* Remove trailing \n if there are any, as logMessage adds it later. */
            if ([message hasSuffix:@"\n"]) {
                message = [message substringToIndex:[message length] - 1];
            }
            [fsMsgConn logMessage:message];
        } else {
            os_log_error(fskit_std_log(), "%s: No message connection object, can't log message", __FUNCTION__);
        }
    } else {
        os_log_error(fskit_std_log(), "%s: Context is null, can't log message", __FUNCTION__);
    }
}

/** Print function to log messages from lib newfs msdos.*/
static void
newfsPrintFunction(newfs_client_ctx_t ctx, int level, const char *fmt, va_list ap)
{
    if(ctx) {
        NewfsCtxAppex *newfsCtx = (NewfsCtxAppex *)ctx;
        if(newfsCtx->fsMsgConn) {
            FSMessageConnection *fsMsgConn = newfsCtx->fsMsgConn;
            NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:ap];
            /* Remove trailing \n if there are any, as logMessage adds it later. */
            if ([message hasSuffix:@"\n"]) {
                message = [message substringToIndex:[message length] - 1];
            }
            [fsMsgConn logMessage:message];
        } else {
            os_log_error(fskit_std_log(), "%s: No message connection object, can't log message", __FUNCTION__);
        }
    } else {
        os_log_error(fskit_std_log(), "%s: Context is null, can't log message", __FUNCTION__);
    }
}

/** WipeFS function, de-references the wipe fs context object, and calls FSSimpleFileSystem wipeResource method */
static int wipeFSCallback(newfs_client_ctx_t ctx, WipeFSProperties wipeFSProps)
{
    NewfsCtxAppex *newfsCtx;
    FSSimpleFileSystem *fsSimpleFS;
    FSResource *resource;
    NSSet<NSString *> *including = nil;
    NSSet<NSString *> *excluding = nil;
    if(!ctx) {
        os_log_error(fskit_std_log(), "%s: Context is null, can't wipe resource", __FUNCTION__);
        return EINVAL;
    }
    newfsCtx = (NewfsCtxAppex *)ctx;
    if (!newfsCtx->fs) {
        os_log_error(fskit_std_log(), "%s: Context isn't initialized, can't wipe resource", __FUNCTION__);
        return EINVAL;
    }
    fsSimpleFS = newfsCtx->fs;
    resource = newfsCtx->resource;
    FSBlockDeviceResource *device = nil;
    if (resource == nil || resource.kind != FSResourceKindBlockDevice) {
        os_log_error(fskit_std_log(), "%s: Given device is not a block device", __FUNCTION__);
        return EINVAL;
    }
    device = [FSBlockDeviceResource dynamicCast:resource];
    if (device.fileDescriptor != wipeFSProps.fd) {
        os_log_error(fskit_std_log(), "%s: Given resource (%d) and file descriptor (%d) aren't the same, can't preform wipefs", __FUNCTION__, device.fileDescriptor, wipeFSProps.fd);
        return EINVAL;
    }
    if (wipeFSProps.include_block_length) {
        including = [NSSet setWithObject:NSStringFromRange(NSMakeRange(wipeFSProps.include_block_start, wipeFSProps.include_block_length))];
    }
    if (wipeFSProps.except_block_length) {
        excluding = [NSSet setWithObject:NSStringFromRange(NSMakeRange(wipeFSProps.except_block_start, wipeFSProps.except_block_length))];
    }
    __block NSError *error = nil;
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_enter(group);
    [fsSimpleFS wipeResource:resource
             includingRanges:including
             excludingRanges:excluding
                       reply:^(NSError * _Nullable err) {
        error = err;
        if (error) {
            os_log_error(fskit_std_log(), "%s: got reply from send wipe resource request with err: %@", __FUNCTION__, error);
        } else {
            os_log_debug(fskit_std_log(), "%s: got reply from send wipe resource request with no errors", __FUNCTION__);
        }
        dispatch_group_leave(group);
    }];
    os_log_debug(fskit_std_log(), "%s: waiting for reply from send wipe resource request", __FUNCTION__);
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    if (error) {
        os_log_error(fskit_std_log(), "%s: Wipe resource error: %s", __FUNCTION__, [[error description] UTF8String]);
        return (int)error.code;
    }
    return 0;
}

@implementation msdosFileSystem

-(void)didFinishLaunching
{
    os_log_info(fskit_std_log(), "%s: Finished launching", __FUNCTION__);
}

-(void)loadVolume:(FSResource *)device
            reply:(void(^)(FSVolume * _Nullable newVolume, NSError * _Nullable error))reply
{
    return reply(nil, fs_errorForPOSIXError(ENOTSUP));
}

-(void)didFinishLoading
{
    os_log_info(fskit_std_log(), "%s: Finished loading", __FUNCTION__);
}

-(NSError *_Nullable)syncRead:(FSBlockDeviceResource *)device
                         into:(void *)buffer
                   startingAt:(off_t)offset
                       length:(size_t)nbyte
{
    __block NSError *error = nil;

    [device synchronousReadInto:buffer
                     startingAt:offset
                         length:nbyte
                          reply:^(size_t actuallyRead, NSError * _Nullable innerError) {
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

-(BOOL)isLabelLegal:(char *)label
{
    int i = 0;
    int c = 0;

    for (i = 0, c = 0; i < LABEL_LENGTH; i++) {
        c = (u_char)label[i];
        /* First charachter can't be a blank space */
        if (c < ' ' + !i || strchr("\"*+,./:;<=>?[\\]|", c)) {
            return false;
        }
    }
    return true;
}

-(CFStringEncoding)getDefaultDOSEncoding
{
    CFStringEncoding encoding = kCFStringEncodingMacRoman; /* Default */
    char buffer[MAXPATHLEN + 1] = {0};
    struct passwd *passwdp = NULL;
    ssize_t size = 0;
    int fd = -1;

    if ((passwdp = getpwuid(getuid()))) {
        strlcpy(buffer, passwdp->pw_dir, sizeof(buffer));
        strlcpy(buffer, passwdp->pw_dir, sizeof(buffer));

        if ((fd = open(buffer, O_RDONLY, 0)) > 0) {
            size = read(fd, buffer, MAXPATHLEN);
            buffer[(size < 0 ? 0 : size)] = '\0';
            close(fd);
            encoding = (CFStringEncoding)strtol(buffer, NULL, 0);
        }
    }

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

- (NSString*)getVolumeName:(FSBlockDeviceResource *)device
                       bps:(uint16_t)bps
                       spc:(uint8_t)spc
                bootsector:(union bootsector * _Nonnull)bootSector
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

    rootDirSectors = ((getuint16(b50->bpbRootDirEnts) * sizeof(struct dosdirentry)) + (bps-1)) / bps;
    if (rootDirSectors) {
        /* This is FAT12/16 */
        char rootdirbuf[MAX_DOS_BLOCKSIZE];
        unsigned firstRootDirSecNum;
        int j = 0;

        firstRootDirSecNum = getuint16(b33->bpbResSectors) + (b33->bpbFATs * getuint16(b33->bpbFATsecs));
        for (i=0; i< rootDirSectors; i++) {
            error = [self syncRead:device
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
        uint8_t *rootDirBuffer = (uint8_t*)malloc(bytesPerCluster);
        uint32_t cluster = getuint32(b710->bpbRootClust);
        off_t readOffset;

        if (!rootDirBuffer) {
            os_log_error(fskit_std_log(), "%s: failed to malloc rootDirBuffer\n", __FUNCTION__);
            error = fs_errorForPOSIXError(ENOMEM);
        }

        while (!finished && cluster >= CLUST_FIRST && cluster < CLUST_RSRVD) {
            /* Find sector where clusters start */
            readOffset = getuint16(b710->bpbResSectors) + (b710->bpbFATs * getuint32(b710->bpbBigFATsecs));
            /* Find sector where "cluster" starts */
            readOffset += ((off_t) cluster - CLUST_FIRST) * (off_t) spc;
            /* Convert to byte offset */
            readOffset *= (off_t) bps;

            /* Read the cluster */
            error = [self syncRead:device
                               into:rootDirBuffer
                         startingAt:readOffset
                             length:bytesPerCluster];
            if (error != nil) {
                return nil;
            }
            dirp = (struct dosdirentry *)rootDirBuffer;

            /* iterate the directory entries looking for volume label */
            for (i = 0; i < bytesPerCluster; i += sizeof(struct dosdirentry), dirp++) {
                if ((dirp)->deName[0] == SLOT_EMPTY) {
                    finished = true;
                    break;
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
            error = [self syncRead:device
                               into:rootDirBuffer
                         startingAt:readOffset
                             length:bps];
            if (error != nil) {
                return nil;
            }

            cluster = getuint32(rootDirBuffer + ((cluster * 4) % bps));
            cluster &= 0x0FFFFFFF; /* Ignore reserved upper bits */
        }
        free(rootDirBuffer);

        /* If volume label wasn't found, look in the boot blocks */
        if (diskLabel[0] == 0) {
            if (getuint16(b50->bpbRootDirEnts) == 0) {
                /* FAT32 */
                if (((struct extboot*)bootSector->bs710.bsExt)->exBootSignature == EXBOOTSIG) {
                    strncpy(diskLabel, (char *)((struct extboot *)bootSector->bs710.bsExt)->exVolumeLabel, LABEL_LENGTH);
                }
            } else if (((struct extboot *)bootSector->bs50.bsExt)->exBootSignature == EXBOOTSIG) {
                strncpy(diskLabel, (char *)((struct extboot *)bootSector->bs50.bsExt)->exVolumeLabel, LABEL_LENGTH);
            }
        }
    }
    /* Set the file system name */

    /* Convert leading 0x05 to 0xE5 for multibyte languages like Japanese */
    if (diskLabel[0] == 0x05) {
        diskLabel[0] = 0x0E5;
    }

    /* Check for illegal characters */
    if (![self isLabelLegal: diskLabel]) {
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

-(NSUUID *)getVolumeUUID:(union bootsector * _Nonnull)bootSector
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

-(void)probeResource:(FSResource *)resource
               reply:(void(^)(FSMatchResult result,
                              NSString * _Nullable name,
                              NSUUID * _Nullable containerUUID,
                              NSError * _Nullable error))reply
{
    FSMatchResult matchResult = FSMatchNotRecognized;
    FSBlockDeviceResource *device;
    void *bootSectorBuffer = NULL;
    unsigned char *volUuid = NULL;
    union bootsector *bootSector;
    __block NSError *error = nil;
    unsigned long blockSize = 0;
    NSString *volName = nil;
    NSUUID *nsuuid = NULL;

    if (resource.kind != FSResourceKindBlockDevice) {
        os_log(fskit_std_log(), "%s: Given device is not a block device", __FUNCTION__);
        goto out;
    }
    device = [FSBlockDeviceResource dynamicCast:resource];
    os_log(fskit_std_log(), "%s: Device matches!", __FUNCTION__);

    blockSize = device.blockSize;

    // Read boot sector
    bootSectorBuffer = malloc(blockSize);
    if (bootSectorBuffer == NULL)
    {
        os_log_error(fskit_std_log(), "%s: failed to malloc pvBootSector\n", __FUNCTION__);
        error = fs_errorForPOSIXError(ENOMEM);
        goto out;
    }

    error = [self syncRead:device
                      into:bootSectorBuffer
                startingAt:0
                    length:blockSize];
    if (error != nil) {
        goto out;
    }

    /*
     * The first three bytes are an Intel x86 jump instruction.  It should be one
     * of the following forms:
     *    0xE9 0x?? 0x??
     *    0xEB 0x?? 0x90
     * where 0x?? means any byte value is OK.
     *
     * Windows doesn't actually check the third byte if the first byte is 0xEB,
     * so we don't either.
     *
     * If Exfat signiture exsits in boot sector, return failure.
     */
    bootSector = (union bootsector *) bootSectorBuffer;
    if (((bootSector->bs50.bsJump[0] != 0xE9) && (bootSector->bs50.bsJump[0] != 0xEB)) ||
        !memcmp(bootSectorBuffer, "\xEB\x76\x90""EXFAT   ", EXFAT_SIGNITURE_LENGTH)) {
        goto out;
    }

    /*
     * It is possible that the above check could match a partition table, or some
     * non-FAT disk meant to boot a PC.
     * Check some more fields for sensible values.
     */
    struct byte_bpb33 *b33 = (struct byte_bpb33 *)bootSector->bs33.bsBPB;
    struct byte_bpb50 *b50 = (struct byte_bpb50 *)bootSector->bs50.bsBPB;
    struct byte_bpb710 *b710 = (struct byte_bpb710 *)bootSector->bs710.bsBPB;

    /* We only work with 512, 1024, 2048, and 4096 byte sectors */
    uint16_t bps = getuint16(b33->bpbBytesPerSec);
    if ((bps < 0x200) || (bps & (bps - 1)) || (bps > MAX_DOS_BLOCKSIZE)) {
        goto out;
    }

    /* Check to make sure valid sectors per cluster */
    uint8_t spc = b33->bpbSecPerClust;
    if ((spc == 0 ) || (spc & (spc - 1))) {
        goto out;
    }

    /* Make sure the number of FATs is OK; on NTFS, this will be zero */
    if (b33->bpbFATs == 0) {
        goto out;
    }

    /* Make sure the total sectors is non-zero */
    if ((getuint16(b33->bpbSectors) == 0) && (getuint32(b50->bpbHugeSectors) == 0)) {
        goto out;
    }

    /* Make sure there is a root directory */
    if ((getuint16(b33->bpbRootDirEnts) == 0) && (getuint32(b710->bpbRootClust) == 0)) {
        goto out;
    }

    /* Get volume name and UUID */
    volName = [self getVolumeName:device bps:bps spc:spc bootsector:bootSector];
    volUuid = (unsigned char*)calloc(16, sizeof(unsigned char));
    if (!volUuid) {
        error = fs_errorForPOSIXError(ENOMEM);
        goto out;
    }
    nsuuid = [self getVolumeUUID:bootSector uuid:volUuid];

    matchResult = FSMatchUsable;

out:
    if (bootSectorBuffer) {
        free(bootSectorBuffer);
    }

    reply(matchResult, volName, nsuuid, error);
}

-(void)checkResource:(FSResource *)resource
             options:(FSTaskOptionsBundle *)options
          connection:(FSMessageConnection *)connection
              taskID:(NSUUID *)taskID
            progress:(NSProgress *)progress
               reply:(void (^)(NSError * _Nullable))reply
{
    FSTaskProgressUpdater *updater = [FSTaskProgressUpdater newWithProgress:progress];
    struct check_context_t context = {0};
    FSBlockDeviceResource *device;
    int preCheckResult = 0;
    int result = 0;

    os_log(fskit_std_log(), "%s: started to check resource", __FUNCTION__);

    progress.totalUnitCount = 100;

    device = [FSBlockDeviceResource dynamicCast:resource];
    fsck_client_ctx_t ctx = (__bridge_retained void *)connection;
    fsck_set_context_properties(fsckPrintFunction, NULL, ctx);
    fsck_set_maxmem(20 * 1024 * 1024);

    for (FSTaskOption* option in options.options) {
        if ([option.option isEqualToString:@"q"]) {
            fsck_set_quick(true);
        } else if ([option.option isEqualToString:@"n"]) {
            fsck_set_alwaysno(true);
            fsck_set_alwaysyes(false);
            fsck_set_preen(false);
        } else if ([option.option isEqualToString:@"y"]) {
            fsck_set_alwaysyes(true);
            fsck_set_alwaysno(false);
            fsck_set_preen(false);
        } else if ([option.option isEqualToString:@"p"]) {
            fsck_set_preen(true);
            fsck_set_alwaysno(false);
            fsck_set_alwaysyes(false);
        } else if ([option.option isEqualToString:@"M"]) {
            int offset;
            size_t maxmem = 0;
            char errorStr[1024] = {0};
            const char * value = [option.optionValue UTF8String];
            if (sscanf(value, "%zi%n", &maxmem, &offset) == 0)
            {
                snprintf(errorStr, sizeof(errorStr), "Size argument '%s' not recognized\n", value);
                fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
                preCheckResult = EINVAL;
                goto exit;
            }
            switch (value[offset])
            {
                case 'M':
                case 'm':
                    maxmem *= 1024;
                    /* Fall through */
                case 'K':
                case 'k':
                    maxmem *= 1024;
                    if (value[offset+1]) {
                        goto bad_multiplier;
                    }
                    break;
                case '\0':
                    break;
                default:
bad_multiplier:
                snprintf(errorStr, sizeof(errorStr), "Size multiplier '%s' not recognized\n", value+offset);
                fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
                preCheckResult = EINVAL;
                goto exit;
            }
            fsck_set_maxmem(maxmem);
        } else {
            char errorStr[1024] = {0};
            snprintf(errorStr, sizeof(errorStr), "Option '%s' not recognized\n", [option.option UTF8String]);
            fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
            preCheckResult = EINVAL;
            goto exit;
        }
    }

    fsck_set_fd([device fileDescriptor]);
    fsck_set_dev([[device bsdName] UTF8String]);

    // No errors found start checking file system.
    reply(nil);

    context.updater = (__bridge_retained void*)updater;
    context.startPhase = startCallback;
    context.endPhase = endCallback;
    result = checkfilesys([[device bsdName] UTF8String], &context);

    if (progress.totalUnitCount > progress.completedUnitCount) {
        progress.completedUnitCount = progress.totalUnitCount;
    }
    CFRelease(context.updater);
    context.updater = NULL;
    fsck_set_fd(-1);
    CFRelease(ctx);
    ctx = NULL;

exit:
    // If error was found before starting to check the filesystem, reply about that error
    if(preCheckResult) {
        reply(fs_errorForPOSIXError(preCheckResult));
    } else {
        [connection completed:result ? fs_errorForPOSIXError(result) : nil
                        reply:^(int res, NSError * _Nullable err) {
        }];
    }

    os_log(fskit_std_log(), "%s: done", __FUNCTION__);
}

-(void)formatResource:(FSResource *)resource
              options:(FSTaskOptionsBundle *)options
           connection:(FSMessageConnection *)connection
               taskID:(NSUUID *)taskID
             progress:(NSProgress *)progress
                reply:(void (^)(NSError * _Nullable))reply
{
    FSTaskProgressUpdater *updater = [FSTaskProgressUpdater newWithProgress:progress];
    FSBlockDeviceResource *device = [FSBlockDeviceResource dynamicCast:resource];
    NSString *localizedFailureReason = nil;
    struct format_context_t context = {0};
    newfs_client_ctx_t client_ctx = NULL;
    NSString *errFormatStr = nil;
    NewfsProperties newfsProps;
    NewfsCtxAppex *newfsCtx;
    int preFormatResult = 0;
    NSString *logMsg = nil;
    char buf[MAXPATHLEN];
    NewfsOptions sopts;
    int bootFD = -1;
    struct stat sb;
    int result = 0;

    os_log(fskit_std_log(), "%s: started to format resource", __FUNCTION__);

    progress.totalUnitCount = 100;

    memset(&sopts, 0, sizeof(sopts));
    memset(&newfsProps, 0, sizeof(newfsProps));
    newfsCtx = malloc(sizeof(NewfsCtxAppex));
    if (!newfsCtx) {
        os_log_error(fskit_std_log(), "%s: Can't allocate a wipe FS context object", __FUNCTION__);
        preFormatResult = ENOMEM;
        goto exit;
    }
    newfsCtx->fsMsgConn = connection;
    newfsCtx->resource = resource;
    newfsCtx->fs = self;

    client_ctx = (void *)newfsCtx;
    
    /* Make sure all newfs library callback functions were set, use defaults if not */
    if (newfs_get_print_function_callback() == NULL) {
        newfs_set_print_function_callback(newfsPrintFunction);
    }
    if (newfs_get_wipefs_function_callback() == NULL) {
        newfs_set_wipefs_function_callback(wipeFSCallback);
    }
    
    newfs_set_client(client_ctx);

    for (FSTaskOption* option in options.options) {
        NSString* key = option.option;
        const char * value = [option.optionValue UTF8String];
        if ([key isEqualToString:@"N"]) {
            sopts.dryRun = 1;
        } else if ([key isEqualToString:@"B"]) {
            sopts.bootStrapFromFile = value;
        } else if ([key isEqualToString:@"F"]) {
            if (strcmp(value, "12") && strcmp(value, "16") && strcmp(value, "32")) {
                errFormatStr = @"Invalid FAT type (%@), must be 12/16 or 32";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, option.optionValue];
                localizedFailureReason = [connection localizedMessage:errFormatStr
                                                                table:@NEWFS_LOC_TABLE
                                                               bundle:[NSBundle bundleForClass:[self class]], option.optionValue];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto exit;
            }
            sopts.FATType = atoi(value);
        } else if ([key isEqualToString:@"I"]) {
            sopts.volumeID = argto4(value, 0, "volume ID");
            sopts.volumeIDFlag = 1;
        } else if ([key isEqualToString:@"O"]) {
            if (strlen(value) > 8) {
                errFormatStr = @"Bad OEM string (%@)";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, option.optionValue];
                localizedFailureReason = [connection localizedMessage:errFormatStr
                                                                table:@NEWFS_LOC_TABLE
                                                               bundle:[NSBundle bundleForClass:[self class]], option.optionValue];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto exit;
            }
            sopts.OEMString = value;
        } else if ([key isEqualToString:@"S"]) {
            sopts.sectorSize = argto2(value, 1, "bytes/sector");
        } else if ([key isEqualToString:@"P"]) {
            sopts.physicalBytes = argto2(value, 1, "physical bytes/sector");
        } else if ([key isEqualToString:@"a"]) {
            sopts.numOfSectorsPerFAT = argto4(value, 1, "sectors/FAT");
        } else if ([key isEqualToString:@"b"]) {
            sopts.blockSize = argtox(value, 1, "block size");
            sopts.clusterSize = 0;
        } else if ([key isEqualToString:@"c"]) {
            sopts.clusterSize = argto1(value, 1, "sectors/cluster");
            sopts.blockSize = 0;
        } else if ([key isEqualToString:@"e"]) {
            sopts.numOfRootDirEnts = argto2(value, 1, "directory entries");
        } else if ([key isEqualToString:@"f"]) {
            sopts.standardFormat = value;
        } else if ([key isEqualToString:@"h"]) {
            sopts.numDriveHeads = argto2(value, 1, "drive heads");
        } else if ([key isEqualToString:@"i"]) {
            sopts.systemSectorLocation = argto2(value, 1, "info sector");
        } else if ([key isEqualToString:@"k"]) {
            sopts.backupSectorLocation = argto2(value, 1, "backup sector");
        } else if ([key isEqualToString:@"m"]) {
            sopts.mediaDescriptor = argto1(value, 0, "media descriptor");
            sopts.mediaDescriptorFlag = 1;
        } else if ([key isEqualToString:@"n"]) {
            sopts.numbOfFATs = argto1(value, 1, "number of FATs");
        } else if ([key isEqualToString:@"o"]) {
            sopts.numOfHiddenSectors = argto4(value, 0, "hidden sectors");
            sopts.hiddenSectorsFlag = 1;
        } else if ([key isEqualToString:@"r"]) {
            sopts.numOfReservedSectors = argto2(value, 1, "reserved sectors");
        } else if ([key isEqualToString:@"s"]) {
            sopts.fsSizeInSectors = argto4(value, 1, "file system size (in sectors)");
        } else if ([key isEqualToString:@"u"]) {
            sopts.numOfSectorsPerTrack = argto2(value, 1, "sectors/track");
        } else if ([key isEqualToString:@"v"]) {
            if (!oklabel(value)) {
                errFormatStr = @"Given volume name (%@) is invalid for this file system";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, option.optionValue];
                localizedFailureReason = [connection localizedMessage:errFormatStr
                                                                table:@NEWFS_LOC_TABLE
                                                               bundle:[NSBundle bundleForClass:[self class]], option.optionValue];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto exit;
            }
            sopts.volumeName = value;
        }
    }

    const char * fname = [device.bsdName UTF8String];

    int fd = [device fileDescriptor];
    if (fstat(fd, &sb)) {
        NSString* errMsg = [NSString stringWithFormat:@"%s: %s", strerror(errno), fname];
        newfsPrintFunction(client_ctx, LOG_ERR, [errMsg UTF8String], NULL);
        preFormatResult = EINVAL;
        goto exit;
    }

    const char *bname = NULL;
    if (sopts.bootStrapFromFile) {
        bname = sopts.bootStrapFromFile;
        if (!strchr(bname, '/')) {
            snprintf(buf, sizeof(buf), "/boot/%s", bname);
            if (!(bname = strdup(buf))) {
                NSString* errMsg = [NSString stringWithFormat:@"%s", strerror(errno)];
                newfsPrintFunction(client_ctx, LOG_ERR, [errMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto exit;
            }
        }
        // XXXKL: Update the commented out code once FSTaskOptionBundle is able to pass file descriptors
        /* if ((bootFD = open(bname, O_RDONLY)) == -1 || fstat(bootFD, &sb)) {
            NSString* errMsg = [NSString stringWithFormat:@"%s: %s", strerror(errno), bname];
            [self print:[errMsg UTF8String] logLevel:LOG_ERR withArgs:nil];
        } */
    }

    reply(nil);

    // Setup the newfs properties
    newfsProps.fd = fd;
    newfsProps.devName = fname;
    newfsProps.partitionBase = device.partitionBase;
    newfsProps.blockSize = (uint32_t)device.blockSize;
    newfsProps.blockCount = device.blockCount;
    newfsProps.physBlockSize = (uint32_t)device.physicalBlockSize;
    newfsProps.bname = bname;
    newfsProps.bootFD = bootFD;
    newfsProps.sb = sb;

    context.updater = (__bridge_retained void*)updater;
    context.startPhase = startCallback;
    context.endPhase = endCallback;
    result = format(sopts, newfsProps, &context);
    if (progress.totalUnitCount > progress.completedUnitCount) {
        progress.completedUnitCount = progress.totalUnitCount;
    }
    CFRelease(context.updater);
    context.updater = NULL;

exit:
    if (newfsCtx) {
        free(newfsCtx);
    }
    // If error was found before starting to check the filesystem, reply about that error
    if(preFormatResult) {
        if (localizedFailureReason) {
            return reply([NSError errorWithDomain:NSPOSIXErrorDomain
                                                 code:preFormatResult
                                             userInfo:@{NSLocalizedFailureReasonErrorKey:localizedFailureReason}]);
        }
        reply(fs_errorForPOSIXError(preFormatResult));
    } else {
        [connection completed:result ? fs_errorForPOSIXError(result) : nil
                        reply:^(int res, NSError * _Nullable err) {
        }];
    }

    /* Be done with the progress */
    os_log(fskit_std_log(), "%s: done", __FUNCTION__);
}

void startCallback(char* description, int64_t parentUnitCount, int64_t totalCount, unsigned int *completedCount, void *updater)
{
    FSTaskProgressUpdater *progressUpdater = (__bridge FSTaskProgressUpdater*)updater;
    NSError *updaterError = nil;

    updaterError = [progressUpdater startPhase:[[NSString alloc] initWithUTF8String:description]
                               parentUnitCount:parentUnitCount
                               phaseTotalCount:totalCount
                              completedCounter:completedCount];
    if (updaterError) {
        os_log_error(fskit_std_log(), "Failed to start phase, error %s",[[updaterError description] UTF8String]);
    }
}

void endCallback(char* description, void *updater)
{
    FSTaskProgressUpdater *progressUpdater = (__bridge FSTaskProgressUpdater*)updater;
    [progressUpdater endPhase:[[NSString alloc] initWithUTF8String:description]];
}

@end
