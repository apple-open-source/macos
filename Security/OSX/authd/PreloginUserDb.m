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
#import <Security/AuthorizationTagsPriv.h>
#import <libaks_filevault.h>

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


@interface PreloginUserDb : NSObject

- (instancetype)init;

- (BOOL) loadWithError:(NSError **)error;

- (NSArray<NSDictionary *> *) users;
- (NSArray<NSDictionary *> *) users:(NSString *)volumeUuid;

@end

typedef void (^AIRDBDACommonCompletionHandler)(DADissenterRef dissenter);
static void _commonDACompletionCallback(DADiskRef disk, DADissenterRef dissenter, void *context)
{
	AIRDBDACommonCompletionHandler handler = (__bridge AIRDBDACommonCompletionHandler)context;
	handler(dissenter);
}

@implementation PreloginUserDb {
	DMManager *_diskMgr;
	id _daSession;
	NSMutableDictionary<NSString*, NSMutableArray*> *_dbDataDict; // NSDictionary indexed by volume UUID (NSString*)
    NSMutableDictionary<NSString*, NSString*> *_dbVolumeGroupMap;
	dispatch_queue_t _queue;
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
	}
	return self;
}

- (BOOL)loadWithError:(NSError **)err
{
    // get all preboot volumes
    NSArray* prebootVolumes = [self allPrebootVolumes];
    if (prebootVolumes.count == 0) {
        os_log_error(AUTHD_LOG, "Failed to get preboot volumes for Prelogin userDB");
        if (err) {
            *err = [NSError errorWithDomain:@"com.apple.authorization" code:-1000 userInfo:@{ NSLocalizedDescriptionKey : @"Failed to get preboot volumes for Prelogin userDB"}];
        }
        return NO;
    }

    NSUUID *uuid = [self currentRecoveryVolumeUUID];
    os_log_info(AUTHD_LOG, "Current Recovery Volume UUID: %{public}@", uuid);

    _dbDataDict = [NSMutableDictionary new];
    _dbVolumeGroupMap = [NSMutableDictionary new];
    [self processPrebootVolumes:prebootVolumes currentRecoveryVolumeUUID:uuid];

    if (_dbDataDict.count == 0 && uuid != nil) {
        os_log(AUTHD_LOG, "No admins found. Try to load all preboot partitions");
        _dbDataDict = [NSMutableDictionary new];
        [self processPrebootVolumes:prebootVolumes currentRecoveryVolumeUUID:nil]; // load admins from ALL preboot partitions
    }

    if (err) {
        *err = nil;
    }
    return YES;
}

- (NSArray<NSDictionary *> *)users
{
    return [self users:nil];
}

- (NSArray<NSDictionary *> *) users:(NSString *)requestedUuid
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
#pragma mark - Private Methods

- (void)processPrebootVolumes:(NSArray*)prebootVolumes currentRecoveryVolumeUUID:(NSUUID *)currentRecoveryVolumeUUID
{
    // process each preboot volume
    for (id prebootVolume in prebootVolumes) {

        // mount the preboot volume. If it fails it could be already mounted. Try to get mountPoint anyway.
        Boolean mounted = [self mountPrebootVolume:prebootVolume];

        // get a mount point of the preboot volume
        NSString* mountPoint = [self mountPointForPrebootVolume:prebootVolume];
        if (!mountPoint) {
            continue;
        }

        // process the preboot volume
        NSDirectoryEnumerator *dirEnumerator = [[NSFileManager defaultManager] enumeratorAtURL:[NSURL fileURLWithPath:mountPoint isDirectory:YES] includingPropertiesForKeys:nil options:NSDirectoryEnumerationSkipsSubdirectoryDescendants errorHandler:nil];
        for (NSURL *url in dirEnumerator) {
            BOOL isDir = NO;
            [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
            if (!isDir) {
                os_log_info(AUTHD_LOG, "Skipping file %{public}@ (not a directory)", url.path);
                continue;
            }

            NSUUID* volumeUUID =  [[NSUUID alloc] initWithUUIDString:url.lastPathComponent]; // the dir has the name as UUID
            if (!volumeUUID) {
                os_log_info(AUTHD_LOG, "Ignoring folder %{public}@ (not UUID)", url);
                continue;
            }

            if (currentRecoveryVolumeUUID && ![currentRecoveryVolumeUUID isEqualTo:volumeUUID]) {
                os_log_info(AUTHD_LOG, "The preboot volume skipped: %{public}@ (not the currentRecoveryVolumeUUID %{public}@)", url, currentRecoveryVolumeUUID);
                continue;
            }

            [self processVolumeData:volumeUUID mountPoint:mountPoint];
        }

        // unmount the preboot volume
        if (mounted) {
            [self unmountPrebootVolume:prebootVolume];
        }
    }
}

#define kEFISystemVolumeUUIDVariableName "SystemVolumeUUID"
- (NSUUID *)currentRecoveryVolumeUUID
{
    NSData *data;
    NSString * const LANVRAMNamespaceStartupManager = @"5EEB160F-45FB-4CE9-B4E3-610359ABF6F8";
    
    NSString *key = [NSString stringWithFormat:@"%@:%@", LANVRAMNamespaceStartupManager, @kEFISystemVolumeUUIDVariableName];
    
    io_registry_entry_t match = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (match)  {
        CFTypeRef entry = IORegistryEntryCreateCFProperty(match, (__bridge CFStringRef)key, kCFAllocatorDefault, 0);
        IOObjectRelease(match);
        
        if (entry)
        {
            if (CFGetTypeID(entry) == CFDataGetTypeID())
                data = CFBridgingRelease(entry);
            else
                CFRelease(entry);
        }
    }
    os_log_info(AUTHD_LOG, "Current boot volume: %{public}@", data);
    
    if (data) {
        return [[NSUUID alloc] initWithUUIDBytes:data.bytes];
    } else {
        return nil;
    }
}

- (NSArray *)allPrebootVolumes
{
    NSMutableArray* result = [NSMutableArray new];

    DMAPFS* dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];

    for (id tmp in _diskMgr.disks) {
        DADiskRef diskRef = (__bridge DADiskRef)(tmp);
        os_log_info(AUTHD_LOG, "Found disk %{public}@", diskRef);
        
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
            os_log(AUTHD_LOG, "Failed to mount preboot volume %{public}@ (status: 0x%x, reason: \"%{public}@\").", preboot, soft_DADissenterGetStatus(dissenter), soft_DADissenterGetStatusString(dissenter));
        }
        dispatch_semaphore_signal(sem);
    };
    soft_DADiskMount((__bridge DADiskRef _Nonnull)(preboot), NULL, kDADiskMountOptionDefault, _commonDACompletionCallback, (__bridge void * _Nullable)(completionHandler));
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return success;
}

- (NSString *)mountPointForPrebootVolume:(id)preboot
{
    DMDiskErrorType diskErr;
    NSString* result = [_diskMgr mountPointForDisk:(__bridge DADiskRef _Nonnull)(preboot) error:&diskErr];
    if (result) {
        os_log_info(AUTHD_LOG, "Mounted preboot partition %{public}@ at %{public}@", preboot, result);
    } else {
        os_log_error(AUTHD_LOG, "Failed to get preboot mount point: %{public}@", soft_DMUnlocalizedTechnicalErrorString(diskErr));
    }
    return result;
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
    NSString* deviceNode = [self deviceNodeForDisk:localDiskRef];
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
    os_log_info(AUTHD_LOG, "Processing volume data: %{public}@", volumeUuid);
    NSData *vek = [self loadVEKforVolumeWithUUID:volumeUuid mountPoint:mountPoint];
    if (!vek) {
        return;
    }

    DADiskRef cfDiskRef = NULL;
    NSString* deviceNode = [self deviceNodeForVolumeWithUUID:volumeUuid diskRef:&cfDiskRef];
    id diskRef = CFBridgingRelease(cfDiskRef);
    if (!deviceNode) {
        return;
    }
    NSString *volumeGroupUuid;
    DMAPFS* dmAPFS = [[getDMAPFSClass() alloc] initWithManager:_diskMgr];
    DMDiskErrorType diskErr = [dmAPFS volumeGroupForVolume:(__bridge DADiskRef _Nonnull)(diskRef) id:&volumeGroupUuid];
    if (diskErr != kDiskErrorNoError || volumeGroupUuid == nil) {
        os_log_error(AUTHD_LOG, "Error %d while trying to get volume group for %{public}@", diskErr, volumeUuid);
    } else {
	    if ([volumeUuid.UUIDString isEqualTo:volumeGroupUuid]) {
		    NSArray *systemVolumeDisks = nil;
		    diskErr = [dmAPFS disksForVolumeGroup:volumeGroupUuid volumeDisks:nil systemVolumeDisks:&systemVolumeDisks dataVolumeDisks:nil userVolumeDisks:nil container:nil];
		    if (diskErr != kDiskErrorNoError || systemVolumeDisks == nil) {
			    os_log_error(AUTHD_LOG, "Error %d while trying to get volume group disks for %{public}@", diskErr, volumeGroupUuid);
		    } else {
			    // There should be only one systemVolume, but the API returns an array so we'll process as many as it wants to give us
			    for (id tmp in systemVolumeDisks) {
				    DADiskRef systemVolumeDiskRef = (__bridge DADiskRef)(tmp);
				    NSString *systemVolumeUuid = nil;
				    diskErr = [dmAPFS volumeUUIDForVolume:systemVolumeDiskRef UUID:&systemVolumeUuid];
				    if (diskErr != kDiskErrorNoError || systemVolumeUuid == nil) {
					    os_log_error(AUTHD_LOG, "Error %d while trying to get volume uuid disks for some system volumes of group %{public}@", diskErr, volumeGroupUuid);
				    } else {
					    os_log(AUTHD_LOG, "Volume %{public}@ belongs to the group %{public}@", systemVolumeUuid, volumeGroupUuid);
					    _dbVolumeGroupMap[systemVolumeUuid] = volumeGroupUuid;
				    }
			    }
		    }
	    }
    }

    NSDictionary *users = [self loadUserDatabaseForVolumeUUID:volumeUuid mountPoint:mountPoint];
    for (NSString *userName in users) {
        NSDictionary *userData = users[userName];
        os_log_debug(AUTHD_LOG, "Processing user: %{public}@", userData);
        NSString *userGuid = userData[kGUIDItemName];
        if (userGuid == nil) {
            os_log_error(AUTHD_LOG, "Failed to find GUID for user %{public}@", userName);
            continue;
        }
        NSData* kek = [self loadKEKforUuid:userGuid deviceNode:deviceNode];
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
            dict[@PLUDB_SCUNLOCK_DATA] = userData[kSCEnforcementItemName];
        }
        if ([userData.allKeys containsObject:kSCUnlockDataItemName]) {
            dict[@PLUDB_SCUNLOCK_DATA] = userData[kSCUnlockDataItemName];
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
}

@end

OSStatus preloginudb_copy_userdb(const char *uuid, UInt32 flags, CFArrayRef *output)
{
    if (!output) {
        return errAuthorizationBadAddress;
    }
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
    
    os_log_debug(AUTHD_LOG, "Processing user db for volume %{public}s with flags %d", uuid, flags);

    *output = CFBridgingRetain([database users:uuid ? [NSString stringWithUTF8String:uuid]  : nil]);
    return errAuthorizationSuccess;
}
