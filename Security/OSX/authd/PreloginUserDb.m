//
//  PreloginUserDb.m
//  authd
//
//  Copyright © 2019 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <libgen.h>
#import <APFS/APFS.h>
#import <DiskManagement/DiskManagement.h>
#import "PreloginUserDb.h"
#import <LocalAuthentication/LAContext+Private.h>
#import <SoftLinking/SoftLinking.h>
#import "debugging.h"
#import <Security/Authorization.h>
#import <Security/AuthorizationPriv.h>
#import <Security/AuthorizationTagsPriv.h>
#import <libaks_filevault.h>
#import "authutilities.h"
#import <os/boot_mode_private.h>

AUTHD_DEFINE_LOG

SOFT_LINK_FRAMEWORK(Frameworks, LocalAuthentication)
SOFT_LINK_FRAMEWORK(PrivateFrameworks, DiskManagement)
SOFT_LINK_FRAMEWORK(Frameworks, DiskArbitration)
SOFT_LINK_FRAMEWORK(PrivateFrameworks, APFS)

SOFT_LINK_CLASS(LocalAuthentication, LAContext)
SOFT_LINK_CLASS(DiskManagement, DMManager)
SOFT_LINK_CLASS(DiskManagement, DMAPFS)
SOFT_LINK_FUNCTION(APFS, APFSVolumeGetUnlockRecord, soft_APFSVolumeGetUnlockRecord, errno_t, (const char *disk, uuid_t wrecUUID, CFDataRef *data), (disk, wrecUUID, data))
SOFT_LINK_FUNCTION(DiskArbitration, DADiskMount, soft_DADiskMount, void, ( DADiskRef disk, CFURLRef __nullable path, DADiskMountOptions options, DADiskMountCallback __nullable callback, void * __nullable context), (disk, path, options, callback, context ))
SOFT_LINK_FUNCTION(DiskArbitration, DADiskUnmount, soft_DADiskUnmount, void, ( DADiskRef disk, DADiskUnmountOptions options, DADiskUnmountCallback __nullable callback, void * __nullable context), (disk, options, callback, context ))
SOFT_LINK_FUNCTION(DiskArbitration, DADissenterGetStatusString, soft_DADissenterGetStatusString, CFStringRef __nullable, ( DADissenterRef dissenter ), ( dissenter ))
SOFT_LINK_FUNCTION(DiskManagement, DMUnlocalizedTechnicalErrorString, soft_DMUnlocalizedTechnicalErrorString, NSString *, ( DMDiskErrorType inError ), ( inError ))
SOFT_LINK_FUNCTION(DiskArbitration, DASessionCreate, soft_DASessionCreate, DASessionRef __nullable, ( CFAllocatorRef __nullable allocator ), ( allocator ))
SOFT_LINK_FUNCTION(DiskArbitration, DADissenterGetStatus, soft_DADissenterGetStatus, DAReturn, ( DADissenterRef dissenter ), ( dissenter ))
SOFT_LINK_FUNCTION(DiskArbitration, DASessionSetDispatchQueue, soft_DASessionSetDispatchQueue, void, ( DASessionRef session, dispatch_queue_t __nullable queue ), ( session, queue ))

static NSString *kVekItemName = @"SecureAccessToken";
static NSString *kGUIDItemName = @"GeneratedUID";
static NSString *kAuthenticationAuthority = @"AuthenticationAuthority";
static NSString *kIsAdmintemName = @"Admin";
static NSString *kSCUnlockDataItemName = @"FVTokenSecret";
static NSString *kSCEnforcementItemName  = @"SmartCardEnforcement";
static NSString *kSCUacItemName  = @"userAccountControl";

static NSString *kLongNameItemName = @"RealName";
static NSString *kUidItemName = @"UID";
static NSString *kVekFile = @"%@/%@/var/db/secureaccesstoken.plist";
static NSString *kUsersFile = @"%@/%@/var/db/AllUsersInfo.plist";

static NSString *kUsersGUID = @"UserIdent";
static NSString *kUsersNameSection = @"UserNamesData";
static NSString *kUsersSection = @"CryptoUsers";

static NSString *globalConfigPath = @"%@/%@/Library/Preferences";
static NSString *managedConfigPath = @"%@/%@/Library/Managed Preferences";
static NSString *homeDirPath = @"%@/%@/Users";

static NSString *fvunlockOverrideScEnforcementPrefsName = @"com.apple.smartcard.fvunlock";
static NSString *fvunlockOverrideScEnforcementFileName = @"%@/%@/var/db/.scnotenforced";

@interface PreloginUserDb : NSObject {
    Boolean scEnforcementOverriden;
}

- (instancetype)init;

- (BOOL) loadWithError:(NSError **)error;

- (NSArray<NSDictionary *> *) users;
- (NSArray<NSDictionary *> *) users:(NSString *)volumeUuid;
- (NSDictionary *) globalPrefs:(NSString *)uuid domain:(NSString *)domain;
- (NSDictionary *) managedPrefs:(NSString *)uuid domain:(NSString *)domain;
- (NSDictionary *) userPrefs:(NSString *)uuid user:(NSString *)user domain:(NSString *)domain;
@end

typedef void (^AIRDBDACommonCompletionHandler)(DADissenterRef dissenter);
static void _commonDACompletionCallback(DADiskRef disk, DADissenterRef dissenter, void *context)
{
	AIRDBDACommonCompletionHandler handler = (__bridge AIRDBDACommonCompletionHandler)context;
	handler(dissenter);
}

OSStatus preloginDb(PreloginUserDb * _Nonnull * _Nonnull _Nonnulldb);

@implementation PreloginUserDb {
	DMManager *_diskMgr;
	id _daSession;
    NSMutableDictionary<NSString *, NSDictionary *> *_globalPrefs; // NSString *prefDomain indexed by volume UUID (NSString *)
    NSMutableDictionary<NSString *, NSDictionary *> *_userPrefs; // NSString *username indexed by volume UUID (NSString *)
    NSMutableDictionary<NSString *, NSDictionary *> *_managedPrefs; // NSString *prefDomain indexed by volume UUID (NSString *)
	NSMutableDictionary<NSString *, NSMutableArray *> *_dbDataDict; // NSDictionary indexed by volume UUID (NSString *)
    NSMutableDictionary<NSString *, NSString *> *_dbVolumeGroupMap;
	dispatch_queue_t _queue;
    NSArray *_systemVolumePreboots;
}

- (instancetype)init
{
	if ((self = [super init])) {
		_queue = dispatch_queue_create("com.apple.PLUDB", DISPATCH_QUEUE_SERIAL);
		if (!_queue) {
			os_log_error(AUTHD_LOG, "Failed to create queue");
			return nil;
		}

        _diskMgr = [[getDMManagerClass() alloc] init];
		if (!_diskMgr) {
			os_log_error(AUTHD_LOG, "Failed to get DM");
			return nil;
		}

		_daSession = (__bridge_transfer id)soft_DASessionCreate(kCFAllocatorDefault);
		if (!_daSession) {
			os_log_error(AUTHD_LOG, "Failed to get DA");
			return nil;
		}

        soft_DASessionSetDispatchQueue((__bridge DASessionRef _Nullable)_daSession, _queue);
        [_diskMgr setDefaultDASession:(__bridge DASessionRef _Nullable)(_daSession)];
        
        _dbDataDict = [NSMutableDictionary new];
        _dbVolumeGroupMap = [NSMutableDictionary new];
        _userPrefs = [NSMutableDictionary new];
        _globalPrefs = [NSMutableDictionary new];
        _managedPrefs = [NSMutableDictionary new];
	}
	return self;
}

- (BOOL)loadWithError:(NSError **)err
{
    // get all preboot volumes
    NSArray *prebootVolumes = [self relevantPrebootVolumes];
    if (prebootVolumes.count == 0) {
        os_log_error(AUTHD_LOG, "Failed to get preboot volumes for Prelogin userDB");
        if (err) {
            *err = [NSError errorWithDomain:@"com.apple.authorization" code:-1000 userInfo:@{ NSLocalizedDescriptionKey:@"Failed to get preboot volumes for Prelogin user database"}];
        }
        return NO;
    }

    NSUUID *uuid = [self currentRecoveryVolumeUUID];
    os_log_info(AUTHD_LOG, "Current Recovery Volume UUID: %{public}@", uuid);
    
    OSStatus (^volumeWorker)(NSUUID *volumeUuid, NSString *mountPoint) = ^OSStatus(NSUUID *volumeUuid, NSString *mountPoint) {
        [self processVolumeData:volumeUuid mountPoint:mountPoint];
        return noErr;
    };

    [self processPrebootVolumes:prebootVolumes worker:volumeWorker canMount:YES];

    if (err) {
        *err = nil;
    }
    os_log_info(AUTHD_LOG, "AIR initial phase done.");
    return YES;
}

- (NSArray<NSDictionary *> *)users
{
    return [self users:nil];
}

- (NSArray<NSDictionary *> *)users:(NSString *)requestedUuid
{
    if (!_dbDataDict.allValues) {
        return nil;
    }
    if (requestedUuid && !_dbDataDict[requestedUuid]) {
        NSString *realUuid = _dbVolumeGroupMap[requestedUuid];
        if (!realUuid) {
            os_log_info(AUTHD_LOG, "Requested volume %{public}@ was not found and is not volumeGroup", requestedUuid);
            NSArray *keys = [_dbVolumeGroupMap allKeysForObject:requestedUuid];
            for(NSString *uuid in keys) {
                if (_dbDataDict[uuid]) {
                    realUuid = uuid;
                    break;
                }
            }
            if (!realUuid) {
                os_log_info(AUTHD_LOG, "Requested volumeGroup %{public}@ was not found", requestedUuid);
                return nil; // no users for requested partition and no mapping for VolumeGroup or vice versa
            }
        }
        os_log_info(AUTHD_LOG, "Requested volume %{public}@ has no users, trying volume %{public}@", requestedUuid, realUuid);
        requestedUuid = realUuid;
    }
    
    NSMutableArray *allUsers = [NSMutableArray new];
    for (NSString *uuid in _dbDataDict) {
        if (requestedUuid && ![requestedUuid isEqualToString:uuid]) {
            os_log_info(AUTHD_LOG, "Requested volume %{public}@ so ignoring volume %{public}@", requestedUuid, uuid);
            continue;
        }
        [allUsers addObjectsFromArray:_dbDataDict[uuid]];
    }
    return allUsers;
}

- (NSString *)sessionVolumeGroup
{
    static NSString *retval = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *chosenPath = kIODeviceTreePlane ":/chosen";
        io_registry_entry_t chosen = IORegistryEntryFromPath(kIOMasterPortDefault, chosenPath);
        
        if (chosen != IO_OBJECT_NULL)  {
            // Get the boot-uuid
            NSObject *bootUUID = CFBridgingRelease(IORegistryEntryCreateCFProperty(chosen, CFSTR("boot-uuid"), NULL, 0));
            IOObjectRelease(chosen);
            
            // Sanity check the type and convert it to string.
            if ([bootUUID isKindOfClass:[NSData class]]) {
                size_t length = strnlen([(NSData *)bootUUID bytes], [(NSData *)bootUUID length]);
                retval = [[NSString alloc] initWithBytes:[(NSData *)bootUUID bytes] length:length encoding:NSUTF8StringEncoding];
                os_log(AUTHD_LOG, "Boot UUID data %{public}@", retval);
            } else if ([bootUUID isKindOfClass:[NSString class]]) {
                retval = (NSString *)bootUUID;
                os_log(AUTHD_LOG, "Boot UUID string %{public}@", retval);
            } else {
                os_log(AUTHD_LOG, "Boot UUID is missing or not a valid object type.");
            }
            
            if (retval) {
                // is this UUID a volumeGroup?
                DMAPFS *dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];

                // first look if the UUID belongs to the volume group
                NSArray *dataVolumes;
                DMDiskErrorType diskErr = [dmAPFS disksForVolumeGroup:retval volumeDisks:nil systemVolumeDisks:nil dataVolumeDisks:&dataVolumes userVolumeDisks:nil container:nil];
                if (!diskErr && dataVolumes.count) {
                    NSString *uuid;
                    diskErr = [dmAPFS volumeUUIDForVolume:(__bridge DADiskRef _Nonnull)(dataVolumes[0]) UUID:&uuid];
                    if (diskErr) {
                        os_log_error(AUTHD_LOG, "Failed to get UUID for data volume %{public}@: %{public}@", dataVolumes[0], soft_DMUnlocalizedTechnicalErrorString(diskErr));
                    } else {
                        retval = uuid;
                        os_log(AUTHD_LOG, "Using data volume UUID %{public}@", retval);
                    }
                }
            }
            _systemVolumePreboots = [self prebootVolumesForUuid:retval];
        }
    });

    return retval;
}

- (NSDictionary *)globalPrefs:(NSString *)uuid domain:(NSString *)domain
{
    NSDictionary *volumeGlobalPrefs = _globalPrefs[uuid];
    if (!volumeGlobalPrefs) {
        os_log_debug(AUTHD_LOG, "No global prefs for volume %{public}@ were found", uuid);
        return nil;
    }
    return volumeGlobalPrefs[domain];
}

- (NSDictionary *)managedPrefs:(NSString *)uuid domain:(NSString *)domain
{
    NSDictionary *volumeManagedPrefs = _managedPrefs[uuid];
    if (!volumeManagedPrefs) {
        return nil;
    }
    return volumeManagedPrefs[domain];
}

- (NSDictionary *)userPrefs:(NSString *)uuid user:(NSString *)user domain:(NSString *)domain
{
    NSDictionary *volumeUserPrefs = _userPrefs[uuid];
    if (!volumeUserPrefs) {
        os_log_debug(AUTHD_LOG, "No user prefs for volume %{public}@ were found", uuid);
        return nil;
    }
    NSDictionary *userPrefs = volumeUserPrefs[user];
    if (!userPrefs) {
        os_log_debug(AUTHD_LOG, "No user prefs for volume %{public}@ and user %{public}@ were found", uuid, user);
        return nil;
    }
    return userPrefs[domain];
}

- (OSStatus)setEnforcedSmartcardOverride:(NSUUID *)uuid operation:(unsigned char)operation status:(Boolean *)status internal:(Boolean)internal
{
    os_log_info(AUTHD_LOG, "Enforcement operation %d on %{public}@%s", operation, uuid.UUIDString, internal ? " (internal)":"");
    
    if (!isInFVUnlockOrRecovery()) {
        if (operation == kAuthorizationOverrideOperationQuery && !internal) {
            if (status) {
                *status = scEnforcementOverriden;
            }
            return noErr;
        } else if (!internal) {
            os_log_error(AUTHD_LOG, "SmartCard enforcement can be set only from Recovery");
            return errSecNotAvailable;
        }
    }
    
    NSArray *prebootVolumes = [self prebootVolumesForUuid:uuid.UUIDString];
    __block OSStatus retval = errAuthorizationInternal;
    
    OSStatus (^volumeWorker)(NSUUID *volumeUuid, NSString *mountPoint) = ^OSStatus(NSUUID *volumeUuid, NSString *mountPoint) {

        if (uuid && [volumeUuid isNotEqualTo:uuid]) {
            os_log_info(AUTHD_LOG, "The preboot volume skipped: %{public}@ (not the expected %{public}@)", volumeUuid, uuid);
            return errInvalidRange;
        }
        
        NSString *usersPath = [NSString stringWithFormat:kUsersFile, mountPoint, volumeUuid.UUIDString];
        if (access(usersPath.UTF8String, F_OK)) {
            os_log_info(AUTHD_LOG, "This preboot volume is not usable for FVUnlock");
            return errSecInvalidItemRef;
        }
        os_log_info(AUTHD_LOG, "Preboot volume %{public}@ is usable for FVUnlock", volumeUuid);

        NSString *filePath = [NSString stringWithFormat:fvunlockOverrideScEnforcementFileName, mountPoint, volumeUuid.UUIDString];
        BOOL overrideFileExists = !access(filePath.UTF8String, F_OK);

        switch(operation) {
            case kAuthorizationOverrideOperationSet: // set
            {
                if (overrideFileExists) {
                    os_log(AUTHD_LOG, "SmartCard enforcement override is already active.");
                    retval = noErr;
                } else {
                    mode_t mode = S_IRUSR | S_IWUSR;
                    int fd = creat(filePath.UTF8String, mode);
                    if (fd != -1) {
                        os_log(AUTHD_LOG, "SmartCard enforcement override turned on");
                        retval = noErr;
                        close(fd);
                    } else {
                        os_log_error(AUTHD_LOG, "Unable to write override file: %d", errno);
                        retval = errno;
                    }
                }
                break;
            }
            case kAuthorizationOverrideOperationReset: // reset
            {
                if (!overrideFileExists) {
                    os_log(AUTHD_LOG, "SmartCard enforcement override not active.");
                    retval = errSecNoSuchAttr;
                } else {
                    retval = (OSStatus)remove(filePath.UTF8String);
                    os_log(AUTHD_LOG, "SmartCard enforcement override turned off: %d", (int)retval);
                }
                break;
            }
            case kAuthorizationOverrideOperationQuery: // status
            {
                if (!status) {
                    break;
                }
                
                *status = overrideFileExists;
                os_log_debug(AUTHD_LOG, "SmartCard enforcement override status %d", overrideFileExists);
                retval = noErr;
                break;
            }
            default: {
                os_log_error(AUTHD_LOG, "Unknown operation %d", operation);
            }
        }
        
        return retval;
    };

    [self processPrebootVolumes:prebootVolumes worker:volumeWorker canMount:!internal];
    
    return retval;
}

#pragma mark - Private Methods

+ (Boolean)fvunlockMode
{
    static Boolean result = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *bootMode = NULL;
        if (os_boot_mode_query(&bootMode) && bootMode != NULL )
        {
            // FVUnlock and Migration are the modes where no AIR support is needed
            result = (strcmp(bootMode, OS_BOOT_MODE_FVUNLOCK) == 0) || (strcmp(bootMode, OS_BOOT_MODE_MIGRATION) == 0);
            os_log(AUTHD_LOG, "FVUnlock session: %d", result);
        }
    });
    
    return result;
}

- (void)processPrebootVolumes:(NSArray*)prebootVolumes worker:(OSStatus (^)(NSUUID *volumeUuid, NSString *mountPoint))workerBlock canMount:(BOOL)canMount
{
    // process each preboot volume
    for (id prebootVolume in prebootVolumes) {

        BOOL weMountedPreboot = NO;
        NSString *mountPoint = [_diskMgr mountPointForDisk:(__bridge DADiskRef _Nonnull)(prebootVolume) error:NULL];
        if (!mountPoint) {
            if (!canMount) {
                os_log_error(AUTHD_LOG, "Volume not mounted %{public}@, skipping", prebootVolume);
                continue;
            }
            weMountedPreboot = [self mountPrebootVolume:prebootVolume];
            if (weMountedPreboot) {
                mountPoint = [_diskMgr mountPointForDisk:(__bridge DADiskRef _Nonnull)(prebootVolume) error:NULL];
            } else {
                os_log_error(AUTHD_LOG, "Unable to mount %{public}@", prebootVolume);
                continue;
            }
        }
        os_log_info(AUTHD_LOG, "Preboot %{public}@ mounted at %{public}@", prebootVolume, mountPoint);

        // process the preboot volume
        NSDirectoryEnumerator *dirEnumerator = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:mountPoint isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
        for (NSURL *url in dirEnumerator) {
            BOOL isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
            if (!isDir) {
                os_log_info(AUTHD_LOG, "Skipping file %{public}@ (not a directory)", url.path);
                continue;
            }

            NSUUID *volumeUUID = [[NSUUID alloc] initWithUUIDString:url.lastPathComponent]; // the dir has the name as UUID
            if (!volumeUUID) {
                os_log_info(AUTHD_LOG, "Ignoring folder %{public}@ (not UUID)", url);
                continue;
            }

            if (workerBlock) {
                workerBlock(volumeUUID, mountPoint);
            }
        }

        // unmount the preboot volume
        if (weMountedPreboot) {
            // check if this preboot is on the volume where system disk is
            BOOL unmount = YES;
            NSString *sessionVolumeGroup = [self sessionVolumeGroup];
            if (sessionVolumeGroup && [_systemVolumePreboots containsObject:prebootVolume]) {
                unmount = NO;
                os_log_debug(AUTHD_LOG, "Not unmounting session preboot");
            }
            if (unmount) {
                [self unmountPrebootVolume:prebootVolume];
            }
        }
    }
}

- (NSArray *)prebootVolumesForUuid:(NSString *)uuid
{
    DMAPFS *dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];
    DADiskRef diskRef = NULL;
    [dmAPFS volumeForVolumeUUID:uuid volume:&diskRef];
    if (!diskRef) {
        os_log_error(AUTHD_LOG, "Failed to determine disk for UUID %{public}@", uuid);
        return nil;
    }
    
    DADiskRef container = NULL;
    DMDiskErrorType diskErr = [dmAPFS containerForVolume:diskRef container:&container];
    if (!container) {
        os_log_error(AUTHD_LOG, "Failed to determine container for UUID %{public}@: %{public}@", uuid, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return nil;
    }
    NSArray *volumes = nil;
    NSMutableArray *result = @[].mutableCopy;
    [dmAPFS volumesForContainer:container volumes:&volumes];
    
    for (id disk in volumes) {
        BOOL preboot;
        diskErr = [dmAPFS isPrebootVolume:(__bridge DADiskRef _Nonnull)(disk) prebootRole:&preboot];
        if (diskErr) {
            os_log_error(AUTHD_LOG, "Failed to determine preboot state for %{public}@: %{public}@", disk, soft_DMUnlocalizedTechnicalErrorString(diskErr));
            continue;
        }
        if (!preboot) {
            os_log_info(AUTHD_LOG, "Not a preboot volume: %{public}@", disk);
            continue;
        }
        os_log_info(AUTHD_LOG, "Will use preboot volume: %{public}@", disk);

        [result addObject:disk];
    }
    return result;
}

// For FVUnlock or Migration, return just the disks from the current container
// For all other cases, return all available disks
- (NSArray *)disksWithPrebootCandidates:(DMAPFS *)dmAPFS
{
    NSArray *retval = _diskMgr.disks;

    // For FVUnlock only, we need to ignore all volumes not belonging to the
    // current container. Only if enumeration fails, use all disks like before
    if (![PreloginUserDb fvunlockMode]) {
        return retval;
    }
    
    NSString *dataVolumeUuid = [self sessionVolumeGroup];
    if (!dataVolumeUuid) {
        // error, should be set in FVUnlock, we have to parse all volumes
        os_log_error(AUTHD_LOG, "Unable to get data volume UUID");
        return retval;
    }
    
    DADiskRef cfDisk;
    DMDiskErrorType diskErr = [dmAPFS volumeForVolumeUUID:dataVolumeUuid volume:&cfDisk];
    id systemDiskRef = CFBridgingRelease(cfDisk);
    if (diskErr) {
        os_log(AUTHD_LOG, "Failed to determine volume for UUID %{public}@: %{public}@", dataVolumeUuid, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return retval;
    }

    diskErr = [dmAPFS containerForVolume:(__bridge DADiskRef _Nonnull)(systemDiskRef) container:&cfDisk];
    id container = CFBridgingRelease(cfDisk);
    if (diskErr) {
        os_log(AUTHD_LOG, "Failed to determine container for volume %{public}@: %{public}@", systemDiskRef, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return retval;
    }
    
    diskErr = [dmAPFS volumesForContainer:(__bridge DADiskRef _Nonnull)(container) volumes:&retval];
    if (diskErr) {
        os_log(AUTHD_LOG, "Failed to get volumes for container %{public}@: %{public}@", container, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return retval;
    }
    
    // fallback if everything fails, use all disks
    return retval ? : _diskMgr.disks;
}

#define kEFISystemVolumeUUIDVariableName "SystemVolumeUUID"
- (NSUUID *)currentRecoveryVolumeUUID
{
    NSData *data;
    NSString *const LANVRAMNamespaceStartupManager = @"5EEB160F-45FB-4CE9-B4E3-610359ABF6F8";
    
    NSString *key = [NSString stringWithFormat:@"%@:%@", LANVRAMNamespaceStartupManager, @kEFISystemVolumeUUIDVariableName];
    
    io_registry_entry_t match = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (match) {
        CFTypeRef entry = IORegistryEntryCreateCFProperty(match, (__bridge CFStringRef)key, kCFAllocatorDefault, 0);
        IOObjectRelease(match);
        
        if (entry) {
            if (CFGetTypeID(entry) == CFDataGetTypeID())
                data = CFBridgingRelease(entry);
            else
                CFRelease(entry);
        }
    }
    os_log_info(AUTHD_LOG, "Current boot volume: %{public}@", data);
    
    if (data) {
        return [[NSUUID alloc] initWithUUIDBytes:data.bytes];
    }
    
    return nil;
}

- (NSArray *)relevantPrebootVolumes
{
    NSMutableArray *result = [NSMutableArray new];

    DMAPFS *dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];

    
    for (id tmp in [self disksWithPrebootCandidates:dmAPFS]) {
        DADiskRef diskRef = (__bridge DADiskRef)(tmp);
        
        BOOL preboot;
        DMDiskErrorType diskErr = [dmAPFS isPrebootVolume:diskRef prebootRole:&preboot];
        if (diskErr) {
            os_log(AUTHD_LOG, "Failed to determine preboot state for %{public}@: %{public}@", diskRef, soft_DMUnlocalizedTechnicalErrorString(diskErr));
            continue;
        }
        if (!preboot) {
            os_log_info(AUTHD_LOG, "Not a preboot volume: %{public}@", diskRef);
            continue;
        }

        id prebootVolume = CFBridgingRelease([_diskMgr copyBooterDiskForDisk:diskRef error:&diskErr]);
        if (prebootVolume) {
            os_log_info(AUTHD_LOG, "Found APFS preboot %{public}@", prebootVolume);
            [result addObject:prebootVolume];
        } else {
            os_log_error(AUTHD_LOG, "Failed to copy preboot for disk %{public}@, err: %{public}@", diskRef, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        }
    }

    return result;
}

- (BOOL)mountPrebootVolume:(id)preboot
{
    __block BOOL success = NO;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    AIRDBDACommonCompletionHandler completionHandler = ^(DADissenterRef dissenter) {
        success = (dissenter == NULL);
        if (dissenter != NULL) {
            os_log_error(AUTHD_LOG, "Failed to mount preboot volume %{public}@ (status: 0x%x, reason: \"%{public}@\").", preboot, soft_DADissenterGetStatus(dissenter), soft_DADissenterGetStatusString(dissenter));
        }
        dispatch_semaphore_signal(sem);
    };
    soft_DADiskMount((__bridge DADiskRef _Nonnull)(preboot), NULL, kDADiskMountOptionDefault, _commonDACompletionCallback, (__bridge void * _Nullable)(completionHandler));
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    os_log_info(AUTHD_LOG, "Mount preboot volume %{public}@ result: %d", preboot, success);

    return success;
}

- (void)unmountPrebootVolume:(id)preboot
{
    soft_DADiskUnmount((__bridge DADiskRef _Nonnull)(preboot), kDADiskUnmountOptionDefault, nil, nil);
    os_log_info(AUTHD_LOG, "Preboot partition unmounted: %{public}@", preboot);
}

- (NSString *)deviceNodeForVolumeWithUUID:(NSUUID *)volumeUuid diskRef:(DADiskRef *)diskRef
{
    DMDiskErrorType diskErr;
    DADiskRef localDiskRef = [_diskMgr copyDiskForVolumeUUID:volumeUuid.UUIDString error:&diskErr];
    if (!localDiskRef) {
        os_log_error(AUTHD_LOG, "Failed to find disk with volume %{public}@: %{public}@", volumeUuid, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return nil;
    }
    if (diskRef) {
        *diskRef = localDiskRef;
        CFRetain(*diskRef);
    }
    os_log_info(AUTHD_LOG, "Found disk %{public}@ with volume UUID %{public}@", localDiskRef, volumeUuid);
    NSString *deviceNode = [self deviceNodeForDisk:localDiskRef];
    CFRelease(localDiskRef);
    return deviceNode;
}

- (NSString *)deviceNodeForDisk:(DADiskRef)diskRef
{
    DMDiskErrorType diskErr;
    NSString *deviceNode = [_diskMgr deviceNodeForDisk:diskRef error:&diskErr];
    if (!deviceNode) {
        os_log_error(AUTHD_LOG, "Failed to find device node for disk %{public}@: %{public}@", diskRef, soft_DMUnlocalizedTechnicalErrorString(diskErr));
        return nil;
    }
    os_log_info(AUTHD_LOG, "Device node found: %{public}@", deviceNode);
    return deviceNode;
}

- (NSData *)loadVEKforVolumeWithUUID:(NSUUID *)volumeUuid mountPoint:(NSString *)mountPoint
{
    NSString *vekPath = [NSString stringWithFormat:kVekFile, mountPoint, volumeUuid.UUIDString];
    NSDictionary *vekDict = [NSDictionary dictionaryWithContentsOfFile:vekPath];
    NSData *vek = vekDict[kVekItemName];
    if (!vek) {
        os_log_error(AUTHD_LOG, "Failed to load DiskToken from %{public}@", vekPath);
        return nil;
    }
    os_log_info(AUTHD_LOG, "Loaded DiskToken from %{public}@", vekPath);
    
    return vek;
}

- (NSData *)loadKEKforUuid:(NSString *)userUuid deviceNode:(NSString *)deviceNode
{
    NSUUID *nsUuid = [[NSUUID alloc] initWithUUIDString:userUuid];
    uuid_t uuid;
    [nsUuid getUUIDBytes:uuid];
    CFDataRef dataCF;
    errno_t err = soft_APFSVolumeGetUnlockRecord(deviceNode.UTF8String, uuid, &dataCF);
    if(err != 0) {
        os_log_error(AUTHD_LOG, "Failed to find SecureToken on device node %{public}@ and UUID %{public}@ (%d)", deviceNode, userUuid, err);
        return nil;
    }
    os_log_info(AUTHD_LOG, "Loaded SecureToken from device node %{public}@", deviceNode);
    
    NSData *kek = CFBridgingRelease(dataCF);
    return kek;
}

- (NSDictionary *)loadUserDatabaseForVolumeUUID:(NSUUID *)volumeUuid mountPoint:(NSString *)mountPoint
{
    NSString *usersPath = [NSString stringWithFormat:kUsersFile, mountPoint, volumeUuid.UUIDString];
    NSDictionary *users = [NSDictionary dictionaryWithContentsOfFile:usersPath];
    if (users.count == 0) {
        os_log_error(AUTHD_LOG, "Failed to find user records in file %{public}@", usersPath);
        return nil;
    }
    os_log_debug(AUTHD_LOG, "Loaded %lu user records from file %{public}@", (unsigned long)users.count, usersPath);
    return users;
}

- (void)processVolumeData:(NSUUID *)volumeUuid mountPoint:(NSString *)mountPoint
{
    // volumeUuid is the name of the folder on a Preboot volume
    // we need to find out what kind of UUID this is
    // it can be either
    // - UUID of the system volume
    // - UUID of the data volume
    // - UUID of the volume group
    
    os_log_info(AUTHD_LOG, "Processing volume data: %{public}@", volumeUuid);
    NSData *vek = [self loadVEKforVolumeWithUUID:volumeUuid mountPoint:mountPoint];
    if (!vek) {
        return;
    }
    
    __block NSString *deviceNode = nil;
    NSArray *systemVolumeDisks = nil;
    NSArray *dataVolumeDisks = nil;
    DMAPFS *dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];

    // first look if the UUID belongs to the volume group
    DMDiskErrorType diskErr = [dmAPFS disksForVolumeGroup:volumeUuid.UUIDString volumeDisks:nil systemVolumeDisks:&systemVolumeDisks dataVolumeDisks:&dataVolumeDisks userVolumeDisks:nil container:nil];
    if (diskErr != kDiskErrorNoError) {
        // not a volumegroup so try to treat it as a single volume
        DADiskRef cfDiskRef = NULL;
        deviceNode = [self deviceNodeForVolumeWithUUID:volumeUuid diskRef:&cfDiskRef];
        CFReleaseNull(cfDiskRef);
        if (!deviceNode) {
            // no volume with such UUID and no volume group with such UUID => fail
            os_log_error(AUTHD_LOG, "Error %d while trying to get volume group disks for %{public}@", diskErr, volumeUuid.UUIDString);
            return;
        }
        os_log_info(AUTHD_LOG, "Direct volume device node %{public}@", deviceNode);
    } else {
        // Volume group was found and now we have to find all the references which might be used when looking
        // for data on this volume group. We will do a map <data|system volume UUID> - <volume group UUID>
        
        // There should be only one systemVolume, but the API returns an array so we'll process as many as it wants to give us
        void (^volumeWorker)(NSArray *volumes, BOOL systemVolume) = ^void(NSArray *volumes, BOOL systemVolume) {
            for (id tmp in volumes) {
                DADiskRef volumeDiskRef = (__bridge DADiskRef)(tmp);
                NSString *volUuid = nil;
                DMDiskErrorType dErr = [dmAPFS volumeUUIDForVolume:(__bridge DADiskRef _Nonnull)(tmp) UUID:&volUuid];
                if (dErr != kDiskErrorNoError || volUuid == nil) {
                    os_log_error(AUTHD_LOG, "Error %{public}@ while trying to get volume uuid disks for some %{public}s volumes of group %{public}@", soft_DMUnlocalizedTechnicalErrorString(diskErr), systemVolume ? "system":"data", volumeUuid.UUIDString);
                } else {
                    if (!systemVolume && !deviceNode) {
                        deviceNode = [self deviceNodeForDisk:volumeDiskRef];
                        os_log_info(AUTHD_LOG, "Data volume device node %{public}@", deviceNode);
                    }
                    os_log_info(AUTHD_LOG, "Volume %{public}@ belongs to the group %{public}@", volUuid, volumeUuid.UUIDString);
                    self->_dbVolumeGroupMap[volUuid] = volumeUuid.UUIDString;
                }
            }
        };
        volumeWorker(systemVolumeDisks, YES);
        volumeWorker(dataVolumeDisks, NO);
        if (!deviceNode) {
            os_log_error(AUTHD_LOG, "No deviceNode was set");
            return;
        }
    }

    // try to find usable users on the volume
    NSDictionary *users = [self loadUserDatabaseForVolumeUUID:volumeUuid mountPoint:mountPoint];
    for (NSString *userName in users) {
        NSDictionary *userData = users[userName];
        os_log_debug(AUTHD_LOG, "Processing user: %{public}@", userData);
        NSString *userGuid = userData[kGUIDItemName];
        if (userGuid == nil) {
            os_log_error(AUTHD_LOG, "Failed to find GUID for user %{public}@", userName);
            continue;
        }
        NSData *kek = [self loadKEKforUuid:userGuid deviceNode:deviceNode];
        if (!kek) {
            os_log_error(AUTHD_LOG, "Failed to find SecureToken for user %{public}@", userName);
            continue;
        }
        
        NSArray *aauthority = userData[kAuthenticationAuthority];
        NSMutableDictionary *dict = @{}.mutableCopy;
        if (aauthority) {
            dict[@PLUDB_SCPAIR] = aauthority;
            os_log_debug(AUTHD_LOG, "Using authority: %{public}@", aauthority);
        }
        
        Boolean owner;
        struct aks_fv_param_s params = {};
        aks_fv_blob_state_s verifier_state = {};
        struct aks_fv_data_s kekData = { .data = (void *)kek.bytes, .len = kek.length };

        int res = aks_fv_get_blob_state(&params, &kekData, &verifier_state);
        if (res) {
            os_log_error(AUTHD_LOG, "Blob state failed: %x", res);
            owner = NO;
        } else {
            owner = ((verifier_state.flags & aks_fv_state_is_owner) == aks_fv_state_is_owner);
        }
        
        dict[@PLUDB_USERNAME] = userName;
        dict[@PLUDB_GUID] = userGuid;
        dict[@PLUDB_ADMIN] = userData[kIsAdmintemName];
        dict[@PLUDB_KEK] = kek;
        dict[@PLUDB_VEK] = vek;
        dict[@PLUDB_DNODE] = deviceNode;
        dict[@PLUDB_OWNER] = @(owner);

        if ([userData.allKeys containsObject:kSCUnlockDataItemName]) {
            dict[@PLUDB_SCUNLOCK_DATA] = userData[kSCUnlockDataItemName];
        }
        if ([userData.allKeys containsObject:kSCEnforcementItemName]) {
            dict[@PLUDB_SCENF] = userData[kSCEnforcementItemName];
        }
        if ([userData.allKeys containsObject:kSCUacItemName]) {
            dict[@PLUDB_SCUAC] = userData[kSCUacItemName];
        }
        if ([userData.allKeys containsObject:kLongNameItemName]) {
            dict[@PLUDB_LUSERNAME] = userData[kLongNameItemName];
        }
        
        NSMutableArray *array = _dbDataDict[volumeUuid.UUIDString];
        if (array == nil) {
            array = [NSMutableArray new];
            if (!array) {
                os_log_error(AUTHD_LOG, "Failed to create users array");
                return;
            }
            _dbDataDict[volumeUuid.UUIDString] = array;
        }
        
        os_log_info(AUTHD_LOG, "Prelogin UserDB added entry: %{public}@", dict);
        [array addObject:dict];
    }
    
    // check for SC override
    scEnforcementOverriden = NO;
    [self setEnforcedSmartcardOverride:volumeUuid operation:kAuthorizationOverrideOperationQuery status:&scEnforcementOverriden internal:YES];
    os_log_info(AUTHD_LOG, "SC enforcement override: %d", scEnforcementOverriden);
    if (!isInFVUnlockOrRecovery()) {
        
        // remove SCenforcement override flag
        if (scEnforcementOverriden) {
            [self setEnforcedSmartcardOverride:volumeUuid operation:kAuthorizationOverrideOperationReset status:nil internal:YES];
        }
    }
    
    if (![PreloginUserDb fvunlockMode]) {
        os_log_debug(AUTHD_LOG, "Not processing prefs");
        return; // do not process prefs when not in FVUnlock
    }
    
    // process preferences
    // global prefs
    NSMutableDictionary *global = @{}.mutableCopy;
    NSString *filePath = [NSString stringWithFormat:globalConfigPath, mountPoint, volumeUuid.UUIDString];

    NSDirectoryEnumerator *dirEnumerator = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:filePath isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
    for (NSURL *url in dirEnumerator) {
        BOOL isDir = NO;
        [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
        if (isDir) {
            os_log_info(AUTHD_LOG, "Skipping dir %{public}@ (not a file)", url.path);
            continue;
        }
        NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:url.path];
        if (prefs) {
            NSString *prefName = url.URLByDeletingPathExtension.lastPathComponent;
            global[prefName] = prefs;
        }
    }
    
    if (scEnforcementOverriden) {
        os_log_info(AUTHD_LOG, "SC enforcement overriden for this boot");
        global[fvunlockOverrideScEnforcementPrefsName] = @{ @"overrideScEnforcement": @YES };
    }

    if (global.count) {
        _globalPrefs[volumeUuid.UUIDString] = global;
    }
    
    // managed prefs
    NSMutableDictionary *managed = @{}.mutableCopy;
    filePath = [NSString stringWithFormat:managedConfigPath, mountPoint, volumeUuid.UUIDString];

    dirEnumerator = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:filePath isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
    for (NSURL *url in dirEnumerator) {
        BOOL isDir = NO;
        [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
        if (isDir) {
            os_log_info(AUTHD_LOG, "Skipping dir %{public}@ (not a file)", url.path);
            continue;
        }
        NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:url.path];
        if (prefs) {
            NSString *prefName = url.URLByDeletingPathExtension.lastPathComponent;
            managed[prefName] = prefs;
        }
    }
    if (managed.count) {
        _managedPrefs[volumeUuid.UUIDString] = managed;
    }
    
    // per user prefs
    NSMutableDictionary *user = @{}.mutableCopy;
    filePath = [NSString stringWithFormat:homeDirPath, mountPoint, volumeUuid.UUIDString];

    dirEnumerator = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:filePath isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
    for (NSURL *url in dirEnumerator) {
        BOOL isDir = NO;
        [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
        if (!isDir) {
            os_log_info(AUTHD_LOG, "Skipping file %{public}@ (not a directory)", url.path);
            continue;
        }

        NSMutableDictionary *userPrefs = @{}.mutableCopy;
        NSString *userName = url.lastPathComponent;
        NSString *userPrefPath = [NSString stringWithFormat:@"%@/Library/Preferences", url.path];
        NSDirectoryEnumerator *dirEnumerator2 = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:userPrefPath isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
        for (NSURL *userUrl in dirEnumerator2) {
            isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:userUrl.path isDirectory:&isDir];
            if (isDir) {
                os_log_info(AUTHD_LOG, "Skipping dir %{public}@ (not a file)", userUrl.path);
                continue;
            }
            NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:userUrl.path];
            if (prefs) {
                NSString *prefName = userUrl.URLByDeletingPathExtension.lastPathComponent;
                userPrefs[prefName] = prefs;
            }
        }
        
        if (userPrefs.count) {
            user[userName] = userPrefs;
        }
    }
    if (user.count) {
        _userPrefs[volumeUuid.UUIDString] = user;
    }
    os_log_debug(AUTHD_LOG, "Global prefs for volume %@: %@", volumeUuid.UUIDString, global);
    os_log_debug(AUTHD_LOG, "Managed prefs for volume %@: %@", volumeUuid.UUIDString, managed);
    os_log_debug(AUTHD_LOG, "User prefs for volume %@: %@", volumeUuid.UUIDString, user);
}

@end

OSStatus preloginDb(PreloginUserDb **db)
{
    static PreloginUserDb *database;
    static OSStatus loadError = errAuthorizationSuccess;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        os_log_info(AUTHD_LOG, "Going to load User DB");

        database = [[PreloginUserDb alloc] init];
        if (!database) {
            loadError = errAuthorizationInvalidSet;
        } else {
            NSError *error;
            if ([database loadWithError:&error]) {
                loadError = (int)error.code;
            }
        }
    });
    
    if (loadError) {
        return loadError;
    }
    if (db) {
        *db = database;
    }
    return noErr;
}

OSStatus preloginudb_copy_userdb(const char *uuid, UInt32 flags, CFArrayRef *output)
{
    if (!output) {
        return errAuthorizationBadAddress;
    }
    PreloginUserDb *database;
    OSStatus retval = preloginDb(&database);
    if (retval) {
        os_log_error(AUTHD_LOG, "Unable to read db");
        return retval;
    }
    
    os_log_debug(AUTHD_LOG, "Processing user db for volume %{public}s with flags %d", uuid, flags);

    *output = CFBridgingRetain([database users:uuid ? [NSString stringWithUTF8String:uuid]  : nil]);
    return errAuthorizationSuccess;
}

OSStatus prelogin_copy_pref_value(const char * _Nullable uuid, const char *user, const char *domain, const char *item, CFTypeRef *output)
{
    if (!output || !uuid) {
        return errAuthorizationBadAddress;
    }
    PreloginUserDb *database;
    OSStatus retval = preloginDb(&database);
    if (retval) {
        os_log_error(AUTHD_LOG, "Unable to read db");
        return retval;
    }
    
    NSDictionary *prefs;
    NSDictionary *managed;
    NSString *_domain = [NSString stringWithUTF8String:domain];
    NSString *_uuid = [NSString stringWithUTF8String:uuid];
    if (user) {
        os_log_debug(AUTHD_LOG, "Reading user pref volume %{public}s %{public}s/%{public}s for user %s", uuid, domain, item, user);
        prefs = [database userPrefs:_uuid user:[NSString stringWithUTF8String:user] domain:_domain];
    } else {
        os_log_debug(AUTHD_LOG, "Reading global pref volume %{public}s %{public}s/%{public}s", uuid, domain, item);
        managed = [database managedPrefs:_uuid domain:_domain];
        prefs = [database globalPrefs:_uuid domain:_domain];
    }
    
    if (!prefs && !managed) {
        os_log_debug(AUTHD_LOG, "No pref found");
        return errAuthorizationInvalidSet;
    }
    
    id value = managed[[NSString stringWithUTF8String:item]];
    if (value) {
        os_log_info(AUTHD_LOG, "Using managed prefs for %{public}s", item);
    } else {
        os_log_debug(AUTHD_LOG, "Using global prefs for %{public}s", item);
        value = prefs[[NSString stringWithUTF8String:item]];
    }
    if (!value) {
        os_log_debug(AUTHD_LOG, "No pref value with name %{public}s was found", item);
        return errAuthorizationInvalidTag;
    }
    
    *output = CFBridgingRetain(value);
    return errAuthorizationSuccess;
}

OSStatus prelogin_smartcardonly_override(const char *uuid, unsigned char operation, Boolean *status)
{
    if (!uuid) {
        os_log_error(AUTHD_LOG, "No volume UUID provided");
        return errSecParam;
    }
    
    NSUUID *volumeUuid = [[NSUUID alloc] initWithUUIDString:[NSString stringWithUTF8String:uuid]];
    if (!volumeUuid) {
        os_log_error(AUTHD_LOG, "Invalid volume UUID provided: %{public}s", uuid);
        return errSecParam;
    }

    PreloginUserDb *database;
    OSStatus retval = preloginDb(&database);
    if (retval) {
        os_log_error(AUTHD_LOG, "Unable to read db");
        return retval;
    }
    
    
    return [database setEnforcedSmartcardOverride:volumeUuid operation:operation status:status internal:NO];
}

