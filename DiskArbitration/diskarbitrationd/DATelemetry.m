//
//  DATelemetry.m
//  diskarbitrationd
//
//  Created by Andrew Tran on 8/8/24.
//

#include <sys/loadable_fs.h>
#include <libproc.h>
#include <sys/codesign.h>
#include <os/feature_private.h>
#if TARGET_OS_OSX
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#endif
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#import "DATelemetry.h"
#import "DADisk.h"
#import "DAFileSystem.h"
#import "DASupport.h"
#import "DALog.h"
#import <Foundation/Foundation.h>

#if TARGET_OS_OSX || TARGET_OS_IOS
#include <CoreAnalytics/CoreAnalytics.h>
#endif

#define DA_TELEMETRY_EVENT_NAME @"com.apple.diskarbitrationd.telemetry"

#define PROCNAME_BUFSIZE 64

/* Internal enums that correspond to CoreAnalytics views - subject to change */

typedef enum DATelemetryFSType {
    DATelemetryFSTypeNone = 0,
    DATelemetryFSTypeMSDOS,
    DATelemetryFSTypeEXFAT,
    DATelemetryFSTypeAPFS,
    DATelemetryFSTypeHFS,
    DATelemetryFSTypeNTFS,
    DATelemetryFSTypeOther,
    DATelemetryFSTypeMSDOSEFI
} DATelemetryFSType;


typedef enum DATelemetryDiskState {
    DATelemetryDiskStateUnknown = 0,
    DATelemetryDiskStateAppeared,
    DATelemetryDiskStateProbing,
    DATelemetryDiskStateRepairing,
    DATelemetryDiskStateMounting,
    DATelemetryDiskStateMounted,
    DATelemetryDiskStateUnrepairable,
    DATelemetryDiskStateRemoved,
    DATelemetryDiskStateWake,
    DATelemetryDiskStateDarkWake,
    DATelemetryDiskStateWakeFromSleep
} DATelemetryDiskState;

typedef enum DATelemetryUnmountApprovalStatus {
    DATelemetryUnmountApproved = 0,
    DATelemetryUnmountDissentedViaAPI,
    DATelemetryUnmountDissentedViaResourceBusy
} DATelemetryUnmountApprovalStatus;

#define DA_TELEMETRY_NUM_OPERATIONS 6

#define DA_TELEMETRY_VOLUME_CLEAN        0x00000001 // If the probed volume is clean
#define DA_TELEMETRY_VOLUME_MOUNTED      0x00000002 // If the terminated volume is mounted
#define DA_TELEMETRY_VOLUME_IS_APPEAR    0x00000004 // If the terminated volume has appeared
#define DA_TELEMETRY_VOLUME_IS_PROBE     0x00000008 // If the terminated volume is in the middle of probe
#define DA_TELEMETRY_VOLUME_IS_FSCK      0x00000010 // If the terminated volume is in the middle of fsck
#define DA_TELEMETRY_VOLUME_IS_MOUNTING  0x00000020 // If the terminated volume is in the middle of mount
#define DA_TELEMETRY_VOLUME_UNREPAIRABLE 0x00000040 // If the terminated volume is unrepairable
#define DA_TELEMETRY_VOLUME_IS_REMOVED   0x00000080 // If the terminated volume is removed already
// macOS-specific wake event telemetry flags
#define DA_TELEMETRY_VOLUME_WAKE         0x00000100 // If the volume is terminated during a full wake
#define DA_TELEMETRY_VOLUME_DARKWAKE     0x00000200 // If the volume is terminated during a dark wake
#define DA_TELEMETRY_VOLUME_SLEEPWAKE    0x00000400 // If the volume is terminated during wake from sleep

typedef enum DATelemetryOperationType {
    DATelemetryOpProbe = 0,
    DATelemetryOpFSCK,
    DATelemetryOpMount,
    DATelemetryOpEject,
    DATelemetryOpRemove,
    DATelemetryOpUnmount
} DATelemetryOperationType;

typedef enum DATelemetryDeviceType {
    DATelemetryDeviceTypeOther = 0,
    DATelemetryDeviceTypeIOUSB,
    DATelemetryDeviceTypeIONVMeFamily,
    DATelemetryDeviceTypeSD,
    DATelemetryDeviceTypeAppleSDXC
} DATelemetryDeviceType;

typedef enum DATelemetryInterconnectType {
    DATelemetryInterconnectTypeOther = 0,
    DATelemetryInterconnectTypeUSB,
    DATelemetryInterconnectTypePCI,
    DATelemetryInterconnectTypePCIe,
    DATelemetryInterconnectTypeFibreChannel,
    DATelemetryInterconnectTypeSCSI,
    DATelemetryInterconnectTypeFirewire,
    DATelemetryInterconnectTypeVirtual,
    DATelemetryInterconnectTypeAppleFabric
} DATelemetryInterconnectType;

typedef struct __DATelemetry {
    DATelemetryOperationType    operationType;
    CFStringRef                 fsType;
    DATelemetryFSImplementation fsImplementation;
    int                         status;
    uint32_t                    volumeFlags;
    uint64_t                    volumeSize;
    uint64_t                    durationNs;
    bool                        unmountForced;
    pid_t                       dissenterPid;
    bool                        dissentedViaAPI;
    bool                        isExternal;
    bool                        automounted;
    DATelemetryDeviceType       deviceType;
    DATelemetryInterconnectType interconnect;
} __DATelemetry;

static NSString *__DA_pidToFirstPartyProcName( pid_t pid )
{
    uint32_t flags;
    int error;
    char procnamebuf[PROCNAME_BUFSIZE];
    
    if ( pid == -1 )
    {
        return @"n/a";
    }
    
    error = csops( pid , CS_OPS_STATUS , &flags , sizeof( flags ) );
    if ( error )
    {
        DALogError( "Unable to get signing information for pid %d: %d", pid, error );
        return @"other";
    }
    
    if ( flags & CS_PLATFORM_BINARY )
    {
        if ( proc_name( pid , procnamebuf , sizeof( procnamebuf ) ) )
        {
            return [[NSString alloc] initWithUTF8String:procnamebuf];
        }
    }
    
    return @"other";
}

static DATelemetryFSType __DA_checkFSTypeName( CFStringRef fsType )
{
    if ( !CFStringCompare( fsType , CFSTR("MSDOS") , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeMSDOS;
    }
    
    if ( !CFStringCompare( fsType , CFSTR("EXFAT") , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeEXFAT;
    }
    
    if ( !CFStringCompare( fsType , CFSTR("APFS") , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeAPFS;
    }
    
    if ( !CFStringCompare( fsType , CFSTR("HFS") , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeHFS;
    }
    
    if ( !CFStringCompare( fsType , CFSTR("NTFS") , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeNTFS;
    }
    
    if ( !CFStringCompare( fsType , CFSTR(DA_TELEMETRY_TYPE_MSDOS_EFI) , kCFCompareCaseInsensitive ) )
    {
        return DATelemetryFSTypeMSDOSEFI;
    }
    
    return DATelemetryFSTypeOther;
}

static DATelemetryFSType __DA_fsTypeToTelemetryValue( CFStringRef fsType )
{
    CFStringRef fsTypeMinusPrefix;
    DATelemetryFSType ret = DATelemetryFSTypeNone;
    
    if ( fsType == NULL )
    {
        return DATelemetryFSTypeNone;
    }
    
    if ( CFStringHasSuffix( fsType , CFSTR("_fskit") ) )
    {
#if TARGET_OS_OSX || TARGET_OS_IOS
        fsTypeMinusPrefix = DSFSKitGetBundleNameWithoutSuffix( fsType );
#else
        fsTypeMinusPrefix = fsType;
#endif
        ret = __DA_checkFSTypeName( fsTypeMinusPrefix );
        CFRelease( fsTypeMinusPrefix );
    }
    else
    {
        ret = __DA_checkFSTypeName( fsType );
    }
    
    return ret;
}

// Determine which path is used for disk operations
static DATelemetryFSImplementation __DA_fsImplementation( CFStringRef fsType , bool isExternal , bool usedFSKit )
{
    if ( usedFSKit )
    {
        return DATelemetryFSImplementationFSKit;
    }
    
    switch ( __DA_fsTypeToTelemetryValue( fsType ) ) {
        case DATelemetryFSTypeMSDOS:
            return DATelemetryFSImplementationUserFS;
        case DATelemetryFSTypeEXFAT:
            return DATelemetryFSImplementationUserFS;
        case DATelemetryFSTypeAPFS:
            return ( isExternal ) ? DATelemetryFSImplementationUserFS : DATelemetryFSImplementationKext;
        case DATelemetryFSTypeHFS:
#if TARGET_OS_OSX
            return DATelemetryFSImplementationKext;
#else
            return ( isExternal ) ? DATelemetryFSImplementationUserFS : DATelemetryFSImplementationKext;
#endif
        case DATelemetryFSTypeNTFS:
            return DATelemetryFSImplementationUserFS;
        default:
            return DATelemetryFSImplementationKext;
    }
}

/* Map any .util or mount status codes to errno */
static int __DA_statusToTelemetryValue( int status )
{
    switch ( status ) {
        case FSUR_UNRECOGNIZED:
            return ENOENT;
        case FSUR_RECOGNIZED:
        case FSUR_IO_SUCCESS:
            return 0;
        case FSUR_IO_FAIL:
            return EIO;
        default:
            return status;
    }
}

static DATelemetryUnmountApprovalStatus __DA_UnmountApprovalStatusToTelemetryValue( int status ,
                                                                                    bool dissentedViaAPI )
{
    if ( !status )
    {
        return DATelemetryUnmountApproved;
    }
    else if ( dissentedViaAPI )
    {
        return DATelemetryUnmountDissentedViaAPI;
    }
    else
    {
        return DATelemetryUnmountDissentedViaResourceBusy;
    }
}

static uint64_t __DATelemetryOperationDuration( __DATelemetry *telemetry )
{
    return ( telemetry->durationNs / NSEC_PER_MSEC );
}

static int __DADiskStateToTelemetryValue( __DATelemetry *telemetry )
{
    DATelemetryDiskState state = DATelemetryDiskStateUnknown;
    
    if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_IS_REMOVED )
    {
        state = DATelemetryDiskStateRemoved;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_WAKE )
    {
        state = DATelemetryDiskStateWake;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_DARKWAKE )
    {
        state = DATelemetryDiskStateDarkWake;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_SLEEPWAKE )
    {
        state = DATelemetryDiskStateWakeFromSleep;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_UNREPAIRABLE )
    {
        state = DATelemetryDiskStateUnrepairable;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_MOUNTED )
    {
        state = DATelemetryDiskStateMounted;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_IS_FSCK )
    {
        state = DATelemetryDiskStateRepairing;
    }
    else if ( ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_IS_MOUNTING ) )
    {
        state = DATelemetryDiskStateMounting;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_IS_PROBE )
    {
        state = DATelemetryDiskStateProbing;
    }
    else if ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_IS_APPEAR )
    {
        state = DATelemetryDiskStateAppeared;
    }
    
    return state;
}

static bool __DAOperationHasDuration( DATelemetryOperationType op )
{
    return op == DATelemetryOpProbe || op == DATelemetryOpFSCK
        || op == DATelemetryOpMount || op == DATelemetryOpUnmount;
}

static bool __DAOperationHasDissenter( DATelemetryOperationType op )
{
    return op == DATelemetryOpEject || op == DATelemetryOpUnmount;
}

static DATelemetryInterconnectType __DAInterconnectTypeToTelemetry( DADiskRef disk )
{
    CFStringRef protocolType;
    
    if ( disk == NULL )
    {
        return DATelemetryInterconnectTypeOther;
    }
    
    protocolType = DADiskGetDescription( disk , kDADiskDescriptionDeviceProtocolKey );
    
    if ( protocolType == NULL )
    {
        return DATelemetryInterconnectTypeOther;
    }
    
    if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeUSB) ,
                           kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeUSB;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypePCI) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypePCI;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypePCIExpress) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypePCIe;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeFibreChannel) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeFibreChannel;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeSCSIParallel) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeSCSI;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeFireWire) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeFirewire;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeVirtual) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeVirtual;
    }
    else if ( !CFStringCompare( protocolType , CFSTR(kIOPropertyPhysicalInterconnectTypeAppleFabric) ,
                                kCFCompareCaseInsensitive ) )
    {
        return DATelemetryInterconnectTypeAppleFabric;
    }
    
    return DATelemetryInterconnectTypeOther;
}

#if TARGET_OS_OSX
/*
 * Check if the device is in dark wake first.
 * Otherwise, check if the device is returning from sleep or hibernation.
 * We consider the disk being handled in response to a wake
 * if the disk's pending sleep flag hasn't been cleared yet.
 */

static uint32_t __DATelemetryGetWakeState( DADiskRef disk )
{
    uint32_t ret = 0;
    IOPMCapabilityBits currentCaps = IOPMConnectionGetSystemCapabilities();
    uint64_t sleepType;
    
    /* If the system isn't considered awake yet, check the sleep types */
    if ( DADiskGetState( disk , _kDADiskStateWillSleep ) )
    {
        if ( IOPMIsADarkWake( currentCaps ) )
        {
            ret = DA_TELEMETRY_VOLUME_DARKWAKE;
        }
        else
        {
            sleepType = __DAGetSleepSubclass();
            
            if ( sleepType == kIOPMSleepTypeHibernate )
            {
                /* This disk is responding to a full wake */
                ret = DA_TELEMETRY_VOLUME_WAKE;
            }
            else
            {
                /* This disk is responding to a wake from sleep (not from hibernation) */
                ret = DA_TELEMETRY_VOLUME_SLEEPWAKE;
            }
        }
    }
    
    return ret;
}
#endif

static NSDictionary *__DATelemetrySerialize( __DATelemetry *telemetry )
{
    NSMutableDictionary *result = [[NSMutableDictionary alloc] init];

    /* Handle shared fields here */
    result[@"operation_type"] = @( telemetry->operationType );
    result[@"fs_type"] = @( __DA_fsTypeToTelemetryValue( telemetry->fsType ) );
    result[@"is_external"] = @( telemetry->isExternal );
    result[@"fs_implementation"] = @( telemetry->fsImplementation );
    result[@"interconnect_type"] = @( telemetry->interconnect );
    
    if ( telemetry->operationType != DATelemetryOpRemove )
    {
        result[@"status_code"] = @( __DA_statusToTelemetryValue( telemetry->status ) );
    }
    
    if ( __DAOperationHasDuration( telemetry->operationType ) )
    {
        result[@"duration_ms"] = @( __DATelemetryOperationDuration( telemetry ) );
    }
    
    if ( __DAOperationHasDissenter( telemetry->operationType ) )
    {
        result[@"dissenter_name"] = __DA_pidToFirstPartyProcName( telemetry->dissenterPid );
    }
    
    /* Handle unique fields */
    switch ( telemetry->operationType ) {
        case DATelemetryOpProbe:
            result[@"volume_clean"] = @( ( telemetry->volumeFlags & DA_TELEMETRY_VOLUME_CLEAN ) != 0 );
            break;
        case DATelemetryOpFSCK:
            result[@"volume_size"] = @( telemetry->volumeSize );
            break;
        case DATelemetryOpMount:
            result[@"automount"] = @( telemetry->automounted );
            break;
        case DATelemetryOpEject:
            if ( telemetry->volumeFlags != 0 )
            {
                result[@"disk_state"] = @( __DADiskStateToTelemetryValue( telemetry ) );
            }
            break;
        case DATelemetryOpRemove:
            result[@"disk_state"] = @( __DADiskStateToTelemetryValue( telemetry ) );
            break;
        case DATelemetryOpUnmount:
            result[@"unmount_forced"] = @( telemetry->unmountForced );
            result[@"approval_status"] = @( __DA_UnmountApprovalStatusToTelemetryValue(
                                              telemetry->status , telemetry->dissentedViaAPI ) );
            break;
        default:
            break;
    }
    
    return result;
}

// Perform all operations with strings first before sending the event lazily to ensure they are still in memory
int DATelemetrySendProbeEvent( int status ,
                               CFStringRef fsType ,
                               DADiskRef disk ,
                               uint64_t durationNs ,
                               int cleanStatus )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    CFStringRef fsImplementation;
    bool isExternal = DADiskIsExternalVolume( disk );
    bool probedWithFSKit = DADiskGetState( disk , _kDADiskStateProbedWithFSKit );
    
    telemetry.operationType = DATelemetryOpProbe;
    telemetry.fsType = fsType;
    telemetry.fsImplementation = __DA_fsImplementation( fsType , isExternal , probedWithFSKit );
    telemetry.status = status;
    telemetry.durationNs = durationNs;
    telemetry.volumeFlags = ( cleanStatus == 0 ) ? DA_TELEMETRY_VOLUME_CLEAN : 0;
    telemetry.isExternal = isExternal;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    
    return 0;
}

int DATelemetrySendFSCKEvent( int status , DADiskRef disk , uint64_t durationNs )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    
    CFNumberRef diskSize = DADiskGetDescription( disk , kDADiskDescriptionMediaSizeKey );
    DAFileSystemRef filesystem = DADiskGetFileSystem( disk );
    CFStringRef kind = NULL;
    uint64_t volumeSize = 0;
    bool isExternal = false;
    bool probedWithFSKit = false;
    
    if ( diskSize )
    {
        volumeSize = ___CFNumberGetIntegerValue( diskSize );
    }
    
    if ( filesystem != NULL )
    {
        kind = DAGetFSTypeWithUUID( filesystem ,
                                    DADiskGetDescription( disk, kDADiskDescriptionVolumeUUIDKey ) );
    }
    
    if ( disk )
    {
        isExternal = DADiskIsExternalVolume( disk );
        probedWithFSKit = DADiskGetState( disk , _kDADiskStateProbedWithFSKit );
    }
    
    telemetry.operationType = DATelemetryOpFSCK;
    telemetry.fsType = kind;
    telemetry.fsImplementation = __DA_fsImplementation( kind , isExternal , probedWithFSKit );
    telemetry.status = status;
    telemetry.durationNs = durationNs;
    telemetry.volumeSize = volumeSize;
    telemetry.isExternal = isExternal;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}

/* fstab mount map entries are considered automounted, not external volumes, and use the kext path */
int DATelemetrySendMountFstabEvent( int status , CFStringRef fsType )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    
    telemetry.operationType = DATelemetryOpMount;
    telemetry.fsType = fsType;
    telemetry.status = status;
    telemetry.durationNs = 0;
    telemetry.automounted = TRUE;
    telemetry.isExternal = FALSE;
    telemetry.fsImplementation = DATelemetryFSImplementationKext;
    telemetry.interconnect = DATelemetryInterconnectTypeOther;
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}

int DATelemetrySendMountEvent( int status ,
                               DADiskRef disk ,
                               DATelemetryFSImplementation mountType ,
                               uint64_t durationNs )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    DAFileSystemRef filesystem;
    CFStringRef fsType = NULL;
    Boolean isExternal = FALSE, automount = FALSE;
    
    if ( disk )
    {
        filesystem = DADiskGetFileSystem( disk );
        isExternal = DADiskIsExternalVolume( disk );
        automount = DADiskGetState( disk , _kDADiskStateMountAutomatic );
        
        if ( filesystem )
        {
            fsType = DAGetFSTypeWithUUID( filesystem ,
                                          DADiskGetDescription( disk, kDADiskDescriptionVolumeUUIDKey ) );
        }
    }
    
    telemetry.operationType = DATelemetryOpMount;
    telemetry.fsType = fsType;
    telemetry.status = status;
    telemetry.durationNs = durationNs;
    telemetry.automounted = automount;
    telemetry.isExternal = isExternal;
    telemetry.fsImplementation = mountType;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}

int DATelemetrySendEjectEvent( int status , DADiskRef disk , pid_t dissenterPid )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    DAFileSystemRef filesystem;
    CFStringRef fsType = NULL;
    DATelemetryFSImplementation implementation = DATelemetryFSImplementationKext;
    bool isExternal = false;
    
    telemetry.operationType = DATelemetryOpEject;
    telemetry.volumeFlags = 0;
    
    if ( disk )
    {
        filesystem = DADiskGetFileSystem( disk );
        isExternal = DADiskIsExternalVolume( disk );
        
        if ( filesystem )
        {
            fsType = DAGetFSTypeWithUUID( filesystem ,
                                          DADiskGetDescription( disk, kDADiskDescriptionVolumeUUIDKey ) );
        }
        
        // Eject telemetry FSImplementation should match unmount and mount telemetry
        if ( DADiskGetState( disk , _kDADiskStateMountedWithFSKit ) )
        {
            implementation = DATelemetryFSImplementationFSKit;
        }
        else if ( DADiskGetState( disk , _kDADiskStateMountedWithUserFS ) )
        {
            implementation = DATelemetryFSImplementationUserFS;
        }
#if TARGET_OS_OSX
        telemetry.volumeFlags |= __DATelemetryGetWakeState( disk );
#endif
    }
    
    telemetry.fsType = fsType;
    telemetry.status = status;
    telemetry.dissenterPid = dissenterPid;
    telemetry.isExternal = isExternal;
    telemetry.fsImplementation = implementation;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );

    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}

int DATelemetrySendTerminationEvent( DADiskRef disk )
{
    DAFileSystemRef filesystem = DADiskGetFileSystem( disk );
    bool diskIsUnrepairable, diskIsMounted, diskIsProbing, isExternal = false, probedWithFSKit = false;
    CFStringRef kind = NULL;
    
    isExternal = DADiskIsExternalVolume( disk );
    probedWithFSKit = DADiskGetState( disk , _kDADiskStateProbedWithFSKit );
    
    /* Use the same unrepairable check as the path for calling DADialogShowDeviceUnrepairable */
    diskIsUnrepairable = DADiskGetState( disk , kDADiskStateStagedUnrepairable )
        && DADiskGetState( disk, kDADiskStateRequireRepair )
        && DADiskGetState( disk, _kDADiskStateMountAutomatic )
        && DADiskGetClaim( disk ) == NULL;
    
    diskIsMounted = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) != NULL;
    diskIsProbing = DADiskGetState( disk , kDADiskStateStagedProbe )
        && DADiskGetState( disk , kDADiskStateCommandActive );
    
    if ( filesystem != NULL )
    {
        kind = DAGetFSTypeWithUUID( filesystem , DADiskGetDescription( disk, kDADiskDescriptionVolumeUUIDKey ) );
    }

    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    uint32_t volumeFlags = 0;
    
    telemetry.operationType = DATelemetryOpRemove;
    telemetry.fsType = kind ;
    telemetry.status = -1;
    telemetry.fsImplementation = __DA_fsImplementation( kind , isExternal , probedWithFSKit ); // should match probe event
    telemetry.isExternal = isExternal;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );
    
    if ( DADiskGetState( disk , kDADiskStateZombie ) )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_IS_REMOVED;
    }
    if ( DADiskGetState( disk , kDADiskStateStagedAppear ) )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_IS_APPEAR;
    }
    if ( diskIsProbing )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_IS_PROBE;
    }
    if ( DADiskGetState( disk , kDADiskStateRequireRepair ) )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_IS_FSCK;
    }
    if ( DADiskGetState( disk , kDADiskStateMountOngoing ) )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_IS_MOUNTING;
    }
    if ( diskIsUnrepairable )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_UNREPAIRABLE;
    }
    if ( diskIsMounted )
    {
        volumeFlags |= DA_TELEMETRY_VOLUME_MOUNTED;
    }
    
#if TARGET_OS_OSX
    volumeFlags |= __DATelemetryGetWakeState( disk );
#endif
    
    telemetry.volumeFlags = volumeFlags;
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}

int DATelemetrySendUnmountEvent( int status ,
                                 DADiskRef disk ,
                                 bool forced ,
                                 pid_t dissenterPid ,
                                 bool dissentedViaAPI ,
                                 uint64_t durationNs )
{
    __DATelemetry telemetry;
    NSDictionary *eventInfo;
    DAFileSystemRef filesystem = DADiskGetFileSystem( disk );
    CFStringRef fsType = NULL;
    DATelemetryFSImplementation implementation = DATelemetryFSImplementationKext;
    
    if ( filesystem != NULL )
    {
        fsType = DAGetFSTypeWithUUID( filesystem ,
                                      DADiskGetDescription( disk, kDADiskDescriptionVolumeUUIDKey ) );
    }
    
    if ( DADiskGetState( disk , _kDADiskStateMountedWithFSKit ) )
    {
        implementation = DATelemetryFSImplementationFSKit;
    }
    else if ( DADiskGetState( disk , _kDADiskStateMountedWithUserFS ) )
    {
        implementation = DATelemetryFSImplementationUserFS;
    }
    
    telemetry.operationType = DATelemetryOpUnmount;
    telemetry.fsType = fsType;
    telemetry.fsImplementation = implementation;
    telemetry.isExternal = DADiskIsExternalVolume( disk );
    telemetry.status = status;
    telemetry.durationNs = durationNs;
    telemetry.unmountForced = forced;
    telemetry.dissenterPid = dissenterPid;
    telemetry.dissentedViaAPI = dissentedViaAPI;
    telemetry.interconnect = __DAInterconnectTypeToTelemetry( disk );
    
    eventInfo = __DATelemetrySerialize( &telemetry );
#if TARGET_OS_OSX || TARGET_OS_IOS
    AnalyticsSendEventLazy( DA_TELEMETRY_EVENT_NAME , ^NSDictionary<NSString *,NSObject *> * _Nullable {
        return eventInfo;
    });
#endif
    return 0;
}
