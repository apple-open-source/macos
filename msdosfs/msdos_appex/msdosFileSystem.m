/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <FSKit/FSResource.h>
#include <sys/_types/_size_t.h>
#import "msdosFileSystem.h"
#import "msdosProgressHelper.h"
#import "bootsect.h"
#import "bpb.h"

#include <stdint.h>
#include <stdlib.h>

#include "newfs_data_types.h"
#include "lib_fsck_msdos.h"
#include "format.h"
#include "dosfs.h"
#include "ext.h"

#define NEWFS_LOC_TABLE "newfs_appex"

#define EXFAT_SIGNATURE_LENGTH (11)

#define TASK_PROGRESS_TIMER_INTERVAL (1 * NSEC_PER_SEC) /* 1 sec interval to track a task's progress */
#define CHECK_MAX_TIME 200                              /* checkfilesys should take 200 seconds or less */

/** Simple struct to hold required resource and filesystem to call wipeFS method from FSUnaryFileSystem */
typedef struct {
    FSTask *fsTask;
    FSBlockDeviceResource* resource;
    FSUnaryFileSystem* fs;
} NewfsCtxAppex;

void startCallback(char* description, int64_t parentUnitCount, int64_t totalCount, unsigned int *completedCount, void *updater);
void endCallback(char* description, void *updater);
size_t readHelper(void *resource, void *buffer, size_t nbytes, off_t offset);
size_t writeHelper(void *resource, void *buffer, size_t nbytes, off_t offset);
off_t lseekHelper(void *resource, off_t offset);
int fstatHelper(void *resour, struct stat *buffer);

/** Print function to log messages from lib fsck msdos.*/
static void
fsckPrintFunction(fsck_client_ctx_t ctx, int level, const char *fmt, va_list ap)
{
    if(ctx) {
        FSTask *fsTask = (__bridge FSTask *) ctx;
        if (fsTask) {
            NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:ap];
            /* Remove trailing \n if there are any, as logMessage adds it later. */
            if ([message hasSuffix:@"\n"]) {
                message = [message substringToIndex:[message length] - 1];
            }
            [fsTask logMessage:message];
        } else {
            os_log_error(fskit_std_log(), "%s: No message connection object, can't log message", __FUNCTION__);
        }
    } else {
        os_log_error(fskit_std_log(), "%s: Context is null, can't log message", __FUNCTION__);
    }
}

static int
fsckAskFunction(fsck_client_ctx_t ctx, int def, const char *fmt, va_list ap)
{
    NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:ap];
    __block BOOL answer = YES;

    if (fsck_preen()) {
        if (fsck_rdonly()) {
            def = 0;
        }
        if (def) {
            fsck_print(fsck_ctx, LOG_INFO, "FIXED\n");
        }
        return def;
    }

    if (fsck_alwaysyes() || fsck_rdonly()) {
        if (!fsck_quiet()) {
            fsck_print(fsck_ctx, LOG_INFO, "%s? %s\n", message.UTF8String, fsck_rdonly() ? "no" : "yes");
        }
        return !fsck_rdonly();
    }

    return (answer == YES);
}

/** Print function to log messages from lib newfs msdos.*/
static void
newfsPrintFunction(newfs_client_ctx_t ctx, int level, const char *fmt, va_list ap)
{
    if(ctx) {
        NewfsCtxAppex *newfsCtx = (NewfsCtxAppex *)ctx;
        if(newfsCtx->fsTask) {
            FSTask *fsTask = newfsCtx->fsTask;
            NSString *message = [[NSString alloc] initWithFormat:[NSString stringWithUTF8String:fmt] arguments:ap];
            /* Remove trailing \n if there are any, as logMessage adds it later. */
            if ([message hasSuffix:@"\n"]) {
                message = [message substringToIndex:[message length] - 1];
            }
            [fsTask logMessage:message];
        } else {
            os_log_error(fskit_std_log(), "%s: No message connection object, can't log message", __FUNCTION__);
        }
    } else {
        os_log_error(fskit_std_log(), "%s: Context is null, can't log message", __FUNCTION__);
    }
}

/** WipeFS function, de-references the wipe fs context object, and calls FSUnaryFileSystem wipeResource method */
int wipeFSCallback(newfs_client_ctx_t ctx, WipeFSProperties wipeFSProps)
{
    NewfsCtxAppex *newfsCtx;
    FSUnaryFileSystem *fsSimpleFS;
    FSBlockDeviceResource *resource;
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
    if (resource == nil) {
        os_log_error(fskit_std_log(), "%s: Given device is not a block device", __FUNCTION__);
        return EINVAL;
    }

    __block NSError *error = nil;
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_enter(group);
    [fsSimpleFS wipeResource:resource
           completionHandler:^(NSError * _Nullable err) {
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

- (void)loadResource:(nonnull FSResource *)resource
             options:(FSTaskParameters *)options
        replyHandler:(nonnull void (^)(FSVolume * _Nullable, NSError * _Nullable))reply
{
    os_log_info(fskit_std_log(), "%s:start", __FUNCTION__);
    _resource = nil;
    if ([resource isKindOfClass:[FSBlockDeviceResource class]]) {
        _resource                       = (FSBlockDeviceResource *)resource;
    }

    for (NSString *opt in options) {
        if ([opt containsString:@"-f"]) {
            return reply(nil, nil);
        }
    }

    __block FSProbeResult *probeResult = nil;
    __block NSError *error = nil;

    [self probeResource:resource
           replyHandler:^(FSProbeResult * _Nullable result,
                          NSError * _Nullable innerError) {
        if (innerError) {
            error = innerError;
        } else {
            probeResult = result;
        }
    }];

    if (error) {
        // Return probe error
        return reply(nil, error);
    }

    if (probeResult.result != FSMatchResultUsable) {
        // Resource can't be used for MSDOS module
        return reply(nil, fs_errorForPOSIXError(EINVAL));
    }

    _volume = [[msdosVolume alloc] initWithResource:resource
                                           volumeID:probeResult.containerID.volumeIdentifier
                                         volumeName:probeResult.name];

    if (_volume != nil) {
        os_log_info(fskit_std_log(), "%s: loaded resource with volume ID (%@)", __FUNCTION__, probeResult.containerID);
        return reply(self.volume, nil);
    } else {
        /* init isn't supposed to fail, assume EIO if it did */
        return reply(nil, fs_errorForPOSIXError(EIO));
    }
}


-(NSError *_Nullable)syncRead:(FSBlockDeviceResource *)device
                         into:(void *)buffer
                   startingAt:(off_t)offset
                       length:(size_t)nbyte
{
    __block NSError *error = nil;

    size_t actuallyRead = [device readInto:buffer
                                startingAt:offset
                                    length:nbyte
                                     error:&error];
    if (error) {
        os_log_error(fskit_std_log(), "%s: Failed to read, error %@", __FUNCTION__, error);
    } else if (actuallyRead != nbyte) {
        os_log_error(fskit_std_log(), "%s: Expected to read %lu bytes, read %lu", __FUNCTION__, nbyte, actuallyRead);
        /*
         * Setting to EIO for now. pread's manpage lists it as a possible
         * errno value:
         * An I/O error occurred while reading from the file system.
         */
        error = fs_errorForPOSIXError(EIO);
    }

    return error;
}

-(void)probeResource:(FSResource *)resource
        replyHandler:(void(^)(FSProbeResult * _Nullable result,
                              NSError * _Nullable error))replyHandler
{
    FSMatchResult matchResult = FSMatchResultNotRecognized;
    NSMutableData *bootSectorBuffer = nil;
    FSBlockDeviceResource *device = nil;
    FSProbeResult *probeResult = nil;
    unsigned char *volUuid = NULL;
    union bootsector *bootSector;
    __block NSError *error = nil;
    unsigned long blockSize = 0;
    NSString *volName = nil;
    NSUUID *nsuuid = NULL;

    if ([resource isKindOfClass:[FSBlockDeviceResource class]]) {
        device = (FSBlockDeviceResource *)resource;
    }
    if (!device) {
        os_log(fskit_std_log(), "%s: Given device is not a block device", __FUNCTION__);
        goto out;
    }

    blockSize = device.blockSize;

    // Read boot sector
    bootSectorBuffer = [[NSMutableData alloc] initWithLength:blockSize];

    error = [self syncRead:device
                      into:bootSectorBuffer.mutableBytes
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
    bootSector = (union bootsector *)bootSectorBuffer.bytes;//bootSectorBuffer;
    if (((bootSector->bs50.bsJump[0] != 0xE9) && (bootSector->bs50.bsJump[0] != 0xEB)) ||
        !memcmp(bootSectorBuffer.bytes, "\xEB\x76\x90""EXFAT   ", EXFAT_SIGNATURE_LENGTH)) {
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
    volName = [Utilities getVolumeName:device
                                   bps:bps
                                   spc:spc
                            bootsector:bootSector
                                 flags:LABEL_FROM_DIRENTRY | LABEL_FROM_BOOTSECT];

    volUuid = (unsigned char*)calloc(16, sizeof(unsigned char));
    if (!volUuid) {
        error = fs_errorForPOSIXError(ENOMEM);
        goto out;
    }
    nsuuid = [Utilities generateVolumeUuid:bootSector uuid:volUuid];

    matchResult = FSMatchResultUsable;
out:
    os_log_debug(fskit_std_log(), "%s: Setting up probeResult (%@)", __FUNCTION__, nsuuid);
    if (error) {
        return replyHandler(nil, error);
    }
    FSContainerIdentifier *containerID = [[FSContainerIdentifier alloc] initWithUUID:nsuuid];
    probeResult = [FSProbeResult resultWithResult:matchResult
                                              name:volName
                                       containerID:containerID];
    return replyHandler(probeResult, error);
}

-(NSProgress * _Nullable)startCheckWithTask:(FSTask *)task
                                 parameters:(FSTaskParameters *)parameters
                                      error:(NSError**)error
{
    NSProgress          *progress = [[NSProgress alloc] init];
    msdosProgressHelper *updater = [[msdosProgressHelper alloc] initWithProgress:progress];
    __block check_context context = {0};
    FSBlockDeviceResource *device = _resource;
    int preCheckResult = 0;
    __block int result = 0;

    os_log(fskit_std_log(), "%s: started to check resource", __FUNCTION__);

    progress.totalUnitCount = 100;

    __block fsck_client_ctx_t ctx = (__bridge_retained void *)task;
    fsck_set_context_properties(fsckPrintFunction, fsckAskFunction, ctx);
    fsck_set_maxmem(20 * 1024 * 1024);

    for (NSUInteger i = 0; i < parameters.count; i++) {
        NSString *option = parameters[i];
        if ([option isEqualToString:@"-q"]) {
            fsck_set_quick(true);
        } else if ([option isEqualToString:@"-n"]) {
            fsck_set_alwaysno(true);
            fsck_set_alwaysyes(false);
            fsck_set_preen(false);
        } else if ([option isEqualToString:@"-y"]) {
            fsck_set_alwaysyes(true);
            fsck_set_alwaysno(false);
            fsck_set_preen(false);
        } else if ([option isEqualToString:@"-p"]) {
            fsck_set_preen(true);
            fsck_set_alwaysno(false);
            fsck_set_alwaysyes(false);
        } else if ([option isEqualToString:@"-M"]) {
            int offset;
            size_t maxmem = 0;
            char errorStr[1024] = {0};
            const char * value;
            if ((i + 1) == parameters.count) {
                snprintf(errorStr, sizeof(errorStr), "Size argument missing\n");
                fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
                preCheckResult = EINVAL;
                goto afterPreCheck;
            }
            option = parameters[++i];
            value = [option UTF8String];
            if (sscanf(value, "%zi%n", &maxmem, &offset) == 0)
            {
                snprintf(errorStr, sizeof(errorStr), "Size argument '%s' not recognized\n", value);
                fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
                preCheckResult = EINVAL;
                goto afterPreCheck;
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
                goto afterPreCheck;
            }
            fsck_set_maxmem(maxmem);
        } else {
            char errorStr[1024] = {0};
            snprintf(errorStr, sizeof(errorStr), "Option '%s' not recognized\n", [option UTF8String]);
            fsckPrintFunction(ctx, LOG_CRIT, errorStr, NULL);
            preCheckResult = EINVAL;
            goto afterPreCheck;
        }
    }

    fsck_set_dev([[device BSDName] UTF8String]);

afterPreCheck:
    // If error was found before starting to check the filesystem, reply about that error
    if(preCheckResult) {
        *error = fs_errorForPOSIXError(preCheckResult);
        os_log(fskit_std_log(), "%s:done:error(%@)", __FUNCTION__, *error);
        return nil;
    }
    // No errors found start checking file system.
    context.updater = (__bridge_retained void*)updater;
    context.startPhase = startCallback;
    context.endPhase = endCallback;
    context.resource = (__bridge_retained void*)_resource;
    context.readHelper = readHelper;
    context.writeHelper = writeHelper;
    context.fstatHelper = fstatHelper;
    dispatch_async(dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        result = checkfilesys([[device BSDName] UTF8String], &context);

        if (progress.totalUnitCount > progress.completedUnitCount) {
            progress.completedUnitCount = progress.totalUnitCount;
        }
        CFRelease(context.updater);
        context.updater = NULL;
        fsck_set_fd(-1);
        CFRelease(ctx);
        ctx = NULL;
        [task didCompleteWithError:result ? fs_errorForPOSIXError(result) : nil];
    });
    os_log(fskit_std_log(), "%s: done", __FUNCTION__);
    return progress;
}

-(NSProgress * _Nullable)startFormatWithTask:(FSTask *)task
                                  parameters:(FSTaskParameters *)parameters
                                       error:(NSError**)error
{
    NSProgress  *progress = [[NSProgress alloc] init];
    msdosProgressHelper *updater = [[msdosProgressHelper alloc] initWithProgress:progress];
    FSBlockDeviceResource *device = _resource;
    __block struct format_context_s context = {0};
    newfs_client_ctx_t client_ctx = NULL;
    NSString *errFormatStr = nil;
    NewfsProperties newfsProps;
    NewfsCtxAppex *newfsCtx;
    int preFormatResult = 0;
    NSString *logMsg = nil;
    char buf[MAXPATHLEN];
    NewfsOptions sopts;
    int bootFD = -1;
    __block int result = 0;
    const char *bname = NULL;
    const char *fname = NULL;

    os_log(fskit_std_log(), "%s: started to format resource", __FUNCTION__);

    progress.totalUnitCount = 100;

    memset(&sopts, 0, sizeof(sopts));
    memset(&newfsProps, 0, sizeof(newfsProps));
    newfsCtx = malloc(sizeof(NewfsCtxAppex));
    if (!newfsCtx) {
        os_log_error(fskit_std_log(), "%s: Can't allocate a wipe FS context object", __FUNCTION__);
        preFormatResult = ENOMEM;
        goto afterPreCheck;
    }
    newfsCtx->fsTask = task;
    newfsCtx->resource = _resource;
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

    for (NSUInteger i = 0; i < parameters.count; i++) {
        NSString *key = parameters[i];
        const char * value = (i + 1) < parameters.count ? parameters[i+1].UTF8String : NULL;
        if ([key isEqualToString:@"-N"]) {
            sopts.dryRun = 1;
        } else if ([key isEqualToString:@"-B"]) {
            sopts.bootStrapFromFile = value;
        } else if ([key isEqualToString:@"-F"]) {
            if (value == NULL) {
            missing_value:
                errFormatStr = @"Option %@ requires a value";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, key];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto afterPreCheck;
            }
            if (strcmp(value, "12") && strcmp(value, "16") && strcmp(value, "32")) {
                errFormatStr = @"Invalid FAT type (%s), must be 12/16 or 32";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, value];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto afterPreCheck;
            }
            sopts.FATType = atoi(value);
        } else if ([key isEqualToString:@"-I"]) {
            if (value == NULL) { goto missing_value; }
            sopts.volumeID = argto4(value, 0, "volume ID");
            sopts.volumeIDFlag = 1;
        } else if ([key isEqualToString:@"-O"]) {
            if (value == NULL) { goto missing_value; }
            if (strlen(value) > 8) {
                errFormatStr = @"Bad OEM string (%s)";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, value];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto afterPreCheck;
            }
            sopts.OEMString = value;
        } else if ([key isEqualToString:@"-S"]) {
            if (value == NULL) { goto missing_value; }
            sopts.sectorSize = argto2(value, 1, "bytes/sector");
        } else if ([key isEqualToString:@"-P"]) {
            if (value == NULL) { goto missing_value; }
            sopts.physicalBytes = argto2(value, 1, "physical bytes/sector");
        } else if ([key isEqualToString:@"-a"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numOfSectorsPerFAT = argto4(value, 1, "sectors/FAT");
        } else if ([key isEqualToString:@"-b"]) {
            if (value == NULL) { goto missing_value; }
            sopts.blockSize = argtox(value, 1, "block size");
            sopts.clusterSize = 0;
        } else if ([key isEqualToString:@"-c"]) {
            if (value == NULL) { goto missing_value; }
            sopts.clusterSize = argto1(value, 1, "sectors/cluster");
            sopts.blockSize = 0;
        } else if ([key isEqualToString:@"-e"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numOfRootDirEnts = argto2(value, 1, "directory entries");
        } else if ([key isEqualToString:@"-f"]) {
            if (value == NULL) { goto missing_value; }
            sopts.standardFormat = value;
        } else if ([key isEqualToString:@"-h"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numDriveHeads = argto2(value, 1, "drive heads");
        } else if ([key isEqualToString:@"-i"]) {
            if (value == NULL) { goto missing_value; }
            sopts.systemSectorLocation = argto2(value, 1, "info sector");
        } else if ([key isEqualToString:@"-k"]) {
            if (value == NULL) { goto missing_value; }
            sopts.backupSectorLocation = argto2(value, 1, "backup sector");
        } else if ([key isEqualToString:@"-m"]) {
            if (value == NULL) { goto missing_value; }
            sopts.mediaDescriptor = argto1(value, 0, "media descriptor");
            sopts.mediaDescriptorFlag = 1;
        } else if ([key isEqualToString:@"-n"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numbOfFATs = argto1(value, 1, "number of FATs");
        } else if ([key isEqualToString:@"-o"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numOfHiddenSectors = argto4(value, 0, "hidden sectors");
            sopts.hiddenSectorsFlag = 1;
        } else if ([key isEqualToString:@"-r"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numOfReservedSectors = argto2(value, 1, "reserved sectors");
        } else if ([key isEqualToString:@"-s"]) {
            if (value == NULL) { goto missing_value; }
            sopts.fsSizeInSectors = argto4(value, 1, "file system size (in sectors)");
        } else if ([key isEqualToString:@"-u"]) {
            if (value == NULL) { goto missing_value; }
            sopts.numOfSectorsPerTrack = argto2(value, 1, "sectors/track");
        } else if ([key isEqualToString:@"-v"]) {
            if (value == NULL) { goto missing_value; }
            if (!oklabel(value)) {
                errFormatStr = @"Given volume name (%s) is invalid for this file system";
                logMsg = [[NSString alloc] initWithFormat:errFormatStr, value];
                newfsPrintFunction(client_ctx, LOG_ERR, [logMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto afterPreCheck;
            }
            sopts.volumeName = value;
        }
    }

    fname = [device.BSDName UTF8String];
    if (sopts.bootStrapFromFile) {
        bname = sopts.bootStrapFromFile;
        if (!strchr(bname, '/')) {
            snprintf(buf, sizeof(buf), "/boot/%s", bname);
            if (!(bname = strdup(buf))) {
                NSString* errMsg = [NSString stringWithFormat:@"%s", strerror(errno)];
                newfsPrintFunction(client_ctx, LOG_ERR, [errMsg UTF8String], NULL);
                preFormatResult = EINVAL;
                goto afterPreCheck;
            }
        }
        // XXXKL: Update the commented out code once FSTaskOptionBundle is able to pass file descriptors
        /* if ((bootFD = open(bname, O_RDONLY)) == -1 || fstat(bootFD, &sb)) {
            NSString* errMsg = [NSString stringWithFormat:@"%s: %s", strerror(errno), bname];
            [self print:[errMsg UTF8String] logLevel:LOG_ERR withArgs:nil];
        } */
    }

afterPreCheck:
    // If error was found before starting to check the filesystem, reply about that error
    if(preFormatResult) {
        if (newfsCtx) {
            free(newfsCtx);
        }
        *error = fs_errorForPOSIXError(preFormatResult);
        return nil;
    }
    // Setup the newfs properties
    newfsProps.devName = fname;
    newfsProps.blockSize = (uint32_t)device.blockSize;
    newfsProps.blockCount = device.blockCount;
    newfsProps.physBlockSize = (uint32_t)device.physicalBlockSize;
    newfsProps.bname = bname;
    newfsProps.bootFD = bootFD;

    context.updater = (__bridge_retained void*)updater;
    context.startPhase = startCallback;
    context.endPhase = endCallback;
    context.resource = (__bridge_retained void*)_resource;
    context.readHelper = readHelper;
    context.writeHelper = writeHelper;
    dispatch_async(dispatch_get_global_queue (DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        result = format(sopts, newfsProps, &context);
        if (progress.totalUnitCount > progress.completedUnitCount) {
            progress.completedUnitCount = progress.totalUnitCount;
        }
        CFRelease(context.updater);
        context.updater = NULL;

        if (newfsCtx) {
            free(newfsCtx);
        }
        [task didCompleteWithError:result ? fs_errorForPOSIXError(result) : nil];
        /* Be done with the progress */
        os_log(fskit_std_log(), "%s: done", __FUNCTION__);
    });
    return progress;
}

void startCallback(char* description, int64_t parentUnitCount, int64_t totalCount, unsigned int *completedCount, void *updater)
{
    if (!description) {
        os_log_error(fskit_std_log(), "%s: Invalid description (null)", __FUNCTION__);
        return;
    }
    msdosProgressHelper *progressUpdater = (__bridge msdosProgressHelper *)updater;
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
    if (!description) {
        os_log_error(fskit_std_log(), "%s: Invalid description (null)", __FUNCTION__);
        return;
    }
    msdosProgressHelper *progressUpdater = (__bridge msdosProgressHelper *)updater;
    [progressUpdater endPhase:[[NSString alloc] initWithUTF8String:description]];
}

size_t readHelper(void *resource, void *buffer, size_t nbytes, off_t offset)
{
    FSBlockDeviceResource *device = (__bridge FSBlockDeviceResource*)resource;
    NSError *error = nil;
    size_t read = 0;
    size_t actuallyRead = [device readInto:buffer
                                startingAt:offset
                                    length:nbytes
                                     error:&error];
    if (error) {
        errno = (int)error.code;
    } else {
        read = actuallyRead;
    }
    return read;
}

size_t writeHelper(void *resource, void *buffer, size_t nbytes, off_t offset)
{
    FSBlockDeviceResource *device = (__bridge FSBlockDeviceResource*)resource;
    NSError *error = nil;
    size_t written = 0;
    size_t actuallyWritten = [device writeFrom:buffer
                                    startingAt:offset
                                        length:nbytes
                                         error:&error];
    if (error) {
        errno = (int)error.code;
    } else {
        written = actuallyWritten;
    }

    return written;
}

off_t lseekHelper(void *resource, off_t offset)
{
    return offset;
}

int fstatHelper(void *resource, struct stat *buffer)
{
    return 0;
}

@end
