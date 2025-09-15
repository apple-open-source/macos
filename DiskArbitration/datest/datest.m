
/*
 * Copyright (c) 2020-2020 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sysexits.h>
#include <Foundation/Foundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <paths.h>
#include <time.h>
#include <CoreAnalytics/CoreAnalytics.h>
#include <bsm/libbsm.h>

#define kDAMaxArgLength 2048

struct clarg {
    short present;
    short hasArg;
    char argument[kDAMaxArgLength];
};


enum {
    kDADummy = 0,
    kDADevice,
    kDAMount,
    kDAOptions,
    kDAMountpath,
    kDAUnmount,
    kDAEject,
    kDAMountApproval,
    kDAEjectApproval,
    kDAUnmountApproval,
    kDADiskAppeared,
    kDADiskDisAppeared,
    kDADiskDescChanged,
    kDARename,
    kDAEncode,
    kDAIdle,
    kDASessionKeepAliveWithDAIdle,
    kDASessionKeepAliveWithDADiskAppeared,
    kDASessionKeepAliveWithDARegisterDiskAppeared,
    kDASessionKeepAliveWithDADiskDescriptionChanged,
    kDAForce,
    kDAWhole,
    kDANoFollow,
    kDAName,
    kDAUseBlockCallback,
    kDASetFSKitAdditions,
    kDARepairRunning,
    kDARepairProgress,
    kDASetDiskAdoption,
    kDATestTelemetry,
    kDAValue,
    kDAUseAuditToken,
    kDAHelp,
    kDALast
} options;



struct clarg actargs[kDALast];

/*
 * To add an option, allocate an enum in enums.h, add a getopt_long entry here,
 * add to main(), add to usage
 */

/* options descriptor */
static struct option opts[] = {
{ "device",                                     required_argument,      0,              kDADevice},
{ "options",                                    required_argument,      0,              kDAOptions},
{ "mountpath",                                  required_argument,      0,              kDAMountpath},
{ "mount",                                      no_argument,            0,              kDAMount },
{ "unmount",                                    no_argument,            0,              kDAUnmount },
{ "eject",                                      no_argument,            0,              kDAEject },
{ "mountApproval",                              no_argument,            0,              kDAMountApproval },
{ "unmountApproval",                            no_argument,            0,              kDAUnmountApproval },
{ "ejectApproval",                              no_argument,            0,              kDAEjectApproval },
{ "rename",                                     no_argument,            0,              kDARename },
{ "encode",                                     no_argument,            0,              kDAEncode },
{ "value",                                      required_argument,      0,              kDAValue},
{ "name",                                       required_argument,      0,              kDAName},
{ "testDiskAppeared",                           no_argument,            0,              kDADiskAppeared },
{ "testDiskDisAppeared",                        no_argument,            0,              kDADiskDisAppeared },
{ "testDiskDescChanged",                        no_argument,            0,              kDADiskDescChanged },
{ "testDAIdle",                                 no_argument,            0,              kDAIdle},
{ "testDASessionKeepAliveWithDAIdle",                  no_argument,            0,              kDASessionKeepAliveWithDAIdle},
{ "testDASessionKeepAliveWithDADiskAppeared",          no_argument,            0,              kDASessionKeepAliveWithDADiskAppeared},
{ "testDASessionKeepAliveWithDARegisterDiskAppeared",  no_argument,            0,              kDASessionKeepAliveWithDARegisterDiskAppeared},
{ "testDASessionKeepAliveWithDADiskDescriptionChanged",no_argument,            0,              kDASessionKeepAliveWithDADiskDescriptionChanged},
{ "force",                                      no_argument,            0,              kDAForce},
{ "whole",                                      no_argument,            0,              kDAWhole},
{ "nofollow",                                   no_argument,            0,              kDANoFollow},
{ "useBlockCallback",                           no_argument,            0,              kDAUseBlockCallback},
{ "testSetFSKitAdditions",                      no_argument,            0,              kDASetFSKitAdditions},
{ "checkRepairRunning",                         no_argument,            0,              kDARepairRunning },
{ "checkRepairProgress",                        no_argument,            0,              kDARepairProgress },
{ "setDiskAdoption",                            required_argument,      0,              kDASetDiskAdoption },
{ "testTelemetry",                              no_argument,            0,              kDATestTelemetry},
{ "useAuditToken",                              no_argument,            0,              kDAUseAuditToken},
{ "help",                                       no_argument,            0,              kDAHelp },
{ 0,                   0,                      0,              0 }
};

#define DA_TELEMETRY_EVENT @"com.apple.diskarbitrationd.telemetry"
#define DA_EXPECTED_MOUNT_EVENTS 3

typedef enum DATelemetryOperationType {
    DATelemetryOpProbe = 0,
    DATelemetryOpFSCK,
    DATelemetryOpMount,
    DATelemetryOpEject,
    DATelemetryOpRemove,
    DATelemetryOpUnmount
} DATelemetryOperationType;

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

typedef enum DATelemetryUnmountApprovalStatus {
    DATelemetryUnmountApproved = 0,
    DATelemetryUnmountDissentedViaAPI,
    DATelemetryUnmountDissentedViaResourceBusy
} DATelemetryUnmountApprovalStatus;

const NSString * const kAnalyticsEventObserverMessageKey = @"message";
const NSString * const kAnalyticsEventObserverNameKey = @"name";

@interface DATestTelemetryObserverDelegate<AnalyticsEventObserverDelegate> : NSObject
@property NSMutableSet<NSString *> *events;
@property uint32_t                  eventCount;
@property uint32_t                  expectedEventCount;
@property bool                      unmountForced;
@property DATelemetryFSType         fsType;

-(id)initWithEvents:(NSSet<NSString *>*)events;
-(void)verifyNumberOfEvents;
-(void)verifyVolumeFSType:(CFStringRef)fsType;

@end

extern char *optarg;
extern int optind;
dispatch_queue_t myDispatchQueue;
dispatch_queue_t telemetryQueue;
AnalyticsEventObserver *telemetryObserver;
DATestTelemetryObserverDelegate *telemetryDelegate;
int done = 0;

@implementation DATestTelemetryObserverDelegate

+(const char *)DATelemetryFSTypeToString:(DATelemetryFSType)fsType
{
    switch ( fsType )
    {
        case DATelemetryFSTypeNone:
            return "none";
        case DATelemetryFSTypeMSDOS:
            return "msdos";
        case DATelemetryFSTypeEXFAT:
            return "exfat";
        case DATelemetryFSTypeAPFS:
            return "apfs";
        case DATelemetryFSTypeHFS:
            return "hfs";
        case DATelemetryFSTypeNTFS:
            return "ntfs";
        case DATelemetryFSTypeMSDOSEFI:
            return "msdos"; // msdos-efi is considered msdos for DADiskDescription purposes.
        default:
            return "other";
    }
}

-(id)initWithEvents:(NSSet<NSString *>*)events
{
    _events = [NSMutableSet setWithSet:events];
    _eventCount = 0;
    _expectedEventCount = 0;
    _fsType = DATelemetryFSTypeNone;
    _unmountForced = NO;
    return self;
}

#define SINGLE_VOLUME_MOUNT_NUM_EVENTS 2
#define SINGLE_VOLUME_REMOVE_NUM_EVENTS 3

-(void)observer:(AnalyticsEventObserver *)observer didEmitMessage:(NSObject *)message
{
    if ( message == nil )
    {
        printf("Could not receive message from analytics event observer\n");
        return;
    }
    if ( ![message isKindOfClass:[NSString class]])
    {
        printf("Analytics message is not a string\n");
        return;
    }
    
    NSData *data = [(NSString *)message dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *event = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    
    if ( event == nil )
    {
        printf("Could not serialize analytics event message\n");
        return;
    }
    
    NSString *eventName = event[kAnalyticsEventObserverNameKey];
    NSDictionary *eventMessage = event[kAnalyticsEventObserverMessageKey];
    
    if ( ![_events containsObject:eventName] )
    {
        printf("Received unexpected event name %s\n", eventName.UTF8String);
        return;
    }
    
    NSLog(@"Got event message: %@", eventMessage);
    
    DATelemetryFSType fsType = ( (NSNumber *) eventMessage[@"fs_type"] ).intValue;
    NSString *dissenterName;
    NSNumber *status;
    
    if ( _expectedEventCount == 0 )
    {
        _fsType = fsType;
    }
    
    switch ( ( (NSNumber *) eventMessage[@"operation_type"] ).intValue ) {
        case DATelemetryOpProbe:
            if ( _expectedEventCount == 0 )
            {
                _expectedEventCount = SINGLE_VOLUME_MOUNT_NUM_EVENTS; // at least probe + mount is expected
                
                if ( ( (NSNumber *) eventMessage[@"status_code"] ).intValue != 0 )
                {
                    _expectedEventCount--; // if probe failed, then the mount will not continue
                }
                
                else if ( !( (NSNumber *) eventMessage[@"volume_clean"] ).boolValue )
                {
                    _expectedEventCount++; // we should see a full fsck if the volume is not clean
                }
            }
            break;
        case DATelemetryOpFSCK:
        case DATelemetryOpMount:
            // fsck and mount should have the same fs type as probe
            if ( _fsType != fsType )
            {
                printf( "Received operation with fs type %s, expected %s\n" ,
                        [DATestTelemetryObserverDelegate DATelemetryFSTypeToString:fsType] ,
                        [DATestTelemetryObserverDelegate DATelemetryFSTypeToString:_fsType] );
            }
            break;
        case DATelemetryOpEject:
            status = (NSNumber *) eventMessage[@"status_code"];
            
            if ( status && status.intValue != 0 )
            {
                // make sure the dissenting process name is populated.
                dissenterName = (NSString *) eventMessage[@"dissenter_name"];
                
                if ( !dissenterName || [dissenterName isEqualToString:@"n/a"] )
                {
                    printf("dissenting process name is not populated\n");
                }
                
                if ( _expectedEventCount == 0 )
                {
                    _expectedEventCount = 1; // Only one failed eject is expected to run
                }
            }
            break;
        case DATelemetryOpRemove:
            if ( _expectedEventCount == 0 )
            {
                // current disk removal, whole disk removal, and eject
                _expectedEventCount = SINGLE_VOLUME_REMOVE_NUM_EVENTS;
            }
            break;
        case DATelemetryOpUnmount:
            if ( _expectedEventCount == 0 )
            {
                _expectedEventCount = 1; // unmount is observed by itself
            }
            if ( _unmountForced != ( (NSNumber *) eventMessage[@"unmount_forced"] ).boolValue )
            {
                // make sure force unmount is reported correctly
                if ( _unmountForced )
                {
                    printf("Telemetry event did not report forced unmount\n");
                }
                else
                {
                    printf("Telemetry event reported forced unmount when none was requested\n");
                }
            }
            if ( ( (NSNumber *) eventMessage[@"approval_status"] ).intValue
                 == DATelemetryUnmountDissentedViaResourceBusy )
            {
                // make sure the dissenting process name is populated.
                dissenterName = (NSString *) eventMessage[@"dissenter_name"];
                
                if ( !dissenterName || [dissenterName isEqualToString:@"n/a"] )
                {
                    printf("dissenting process name is not populated\n");
                }
            }
            break;
        default:
            break;
    }
    
    _eventCount++;
}

-(void)verifyNumberOfEvents
{
    if ( _eventCount != _expectedEventCount )
    {
        printf("Received %u telemetry events, expected %u\n", _eventCount , _expectedEventCount );
    }
    else
    {
        printf("Received expected number of telemetry events!\n");
    }
}

-(void)verifyVolumeFSType:(CFStringRef)fsTypeExpected
{
    const char *fsTypeString = [DATestTelemetryObserverDelegate DATelemetryFSTypeToString:_fsType];
    NSString *bridgedString = (__bridge NSString *) fsTypeExpected;
    
    if (![@(fsTypeString) isEqualToString:bridgedString])
    {
        printf("Telemetry reported disk with type %s, expected %s\n", fsTypeString ,
               bridgedString.UTF8String);
    }
}

@end

@interface DATestProgressObserver : NSObject {
    NSProgress *_progress;
}

@property NSURL *deviceURL;

-(BOOL)verifyProgress;

@end

@implementation DATestProgressObserver

-(NSProgress *)progress
{
    @synchronized (self) {
        return _progress;
    }
}

-(void)setProgress:(NSProgress * _Nullable)progress {
    @synchronized (self) {
        if ( _progress ) {
            [_progress removeObserver:self
                           forKeyPath:@"completedUnitCount"
                              context:nil];
            _progress = nil;
        }
        _progress = progress;
        if (progress) {
            [progress addObserver:self
                       forKeyPath:@"completedUnitCount"
                          options:NSKeyValueObservingOptionNew
                          context:nil];
        }
    }
}

-(void)observeValueForKeyPath:(NSString *)keyPath
                     ofObject:(id)object
                       change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                      context:(void *)context
{
    @synchronized (self) {
        if ( _progress ) {
            printf( "Current progress: %lld/%lld\n" ,
                    _progress.completedUnitCount ,
                    _progress.totalUnitCount );
        }
    }
}

-(BOOL)verifyProgress
{
    BOOL receivedProgress = NO;
    
    if ( ! _progress )
    {
        printf( "Unable to get repair progress for %s\n", _deviceURL.path.UTF8String );
    }
    else if ( ! _progress.completedUnitCount )
    {
        printf( "Did not get non-zero repair progress for %s\n" ,
               _deviceURL.path.UTF8String );
    }
    else
    {
        printf( "Final progress received for %s: %lld/%lld\n",
                _deviceURL.path.UTF8String ,
                _progress.completedUnitCount ,
                _progress.totalUnitCount );
        
        receivedProgress = YES;
    }
    
    if ( _progress )
    {
        [_progress removeObserver:self
                       forKeyPath:@"completedUnitCount"
                          context:nil];
        _progress = nil;
    }
    
    return receivedProgress;
}

@end

static void usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n", getprogname());
    fputs(
"datest --help\n"
"\n"
"datest --mount --device <device> [--options <options> ] [--mountpath <path>] [--useBlockCallback --useAuditToken] [--testTelemetry] [--checkRepairProgress]\n"
"datest --mountApproval --device <device> [--options <options> ] [--mountpath <path>] [--useBlockCallback]\n"
"datest --unmount --device <device> [--force ] [--whole ] [--useBlockCallback  --useAuditToken] [--testTelemetry]\n"
"datest --unmountApproval --device <device> [--force ] [--whole ] [--useBlockCallback] \n"
"datest --eject --device <device> [--useBlockCallback --useAuditToken] [--testTelemetry]\n"
"datest --ejectApproval --device <device> [--useBlockCallback] \n"
"datest --rename --device <device>  --name <name> [--useBlockCallback --useAuditToken]\n"
"datest --testDiskAppeared [--useBlockCallback] \n"
"datest --testDiskDisAppeared --device <device> [--useBlockCallback] \n"
"datest --testDiskDescChanged --device <device> [--useBlockCallback] [--checkRepairRunning]\n"
"datest --testDAIdle  [--useBlockCallback] \n"
"datest --testDASessionKeepAliveWithDAIdle  \n"
"datest --testDASessionKeepAliveWithDADiskAppeared  \n"
"datest --testDASessionKeepAliveWithDARegisterDiskAppeared  \n"
"datest --testDASessionKeepAliveWithDADiskDescriptionChanged \n"
"datest --setDiskAdoption <y/n> --device <device> \n"
#ifdef DA_FSKIT
"datest --testSetFSKitAdditions --device <device> \n"
#endif

"\n"
,
      stderr);
    exit(1);
}

static kern_return_t getCurrentAuditToken( audit_token_t *token )
{
    kern_return_t ret = KERN_NOT_SUPPORTED;
    
    mach_msg_type_number_t infoSize = TASK_AUDIT_TOKEN_COUNT;
    ret = task_info( mach_task_self() , TASK_AUDIT_TOKEN,
                     (task_info_t)token, &infoSize);
    if ( ret != KERN_SUCCESS )
    {
        printf("Could not retrieve audit token");
    }
    
    return ret;
}

static int TranslateDAError( DAReturn errnum )
{
    int    ret;

    if (errnum >= unix_err(0) && errnum <= unix_err(ELAST)) {
        ret = errnum & ~unix_err(0);
    } else {
        ret = errnum;
    }
    return ret;
}


static int validateArguments( int validArgs[], int numOfRequiredArgs, struct clarg actargs[kDALast] )
{
    for (int i =0; i < numOfRequiredArgs; i++)
    {
        if (0 == actargs[validArgs[i]].present){
            usage();
            return 1;
        }
    }
    return 0;
}

void DiskMountCallback( DADiskRef disk, DADissenterRef dissenter, void *context )
{
    
    DAReturn    ret = 0;
    if (dissenter) {
        ret = DADissenterGetStatus(dissenter);
    }
    printf("mount finished with return status %0x \n", TranslateDAError(ret));
    *(OSStatus *)context = TranslateDAError(ret);
    done = 1;
}


DADissenterRef DiskApprovalCallback( DADiskRef disk, void *context )
{
    printf("approval callback received\n");
    *(OSStatus *)context = 0;
    done = 1;
    return NULL;
}


void DiskUnmountCallback ( DADiskRef disk, DADissenterRef dissenter, void *context )
{
    DAReturn    ret = 0;
    if (dissenter) {
        ret = DADissenterGetStatus(dissenter);
    }
    printf("unmount finished with return status %0x \n", TranslateDAError(ret));
    *(OSStatus *)context = TranslateDAError(ret);
    done = 1;
}

void DiskRenameCallback ( DADiskRef disk, DADissenterRef dissenter, void * context )
{
    DAReturn    ret = 0;
    if (dissenter) {
        ret = DADissenterGetStatus(dissenter);
    }
    printf("Rename finished with return status %0x \n", TranslateDAError(ret));
    *(OSStatus *)context = TranslateDAError(ret);
    done = 1;
}

 void DiskEjectCallback( DADiskRef disk, DADissenterRef dissenter, void *context )
{

     DAReturn    ret = 0;
     if (dissenter) {
         ret = DADissenterGetStatus(dissenter);
     }
     printf("eject finished with return status %0x \n", TranslateDAError(ret));
     *(OSStatus *)context = TranslateDAError(ret);
     done = 1;
  
}

static void
DiskAppearedCallback( DADiskRef disk, void *context )
{
    printf("DiskAppearedCallback dispatched\n");
    *(OSStatus *)context = 0;
    done = 1;
}

static void
DiskDisAppearedCallback( DADiskRef disk, void *context )
{
    printf("DiskDisAppearedCallback dispatched\n");
    *(OSStatus *)context = 0;
    done = 1;
}

void DiskDescriptionChangedCallback( DADiskRef disk, CFArrayRef keys, void *context )
{
    CFRetain(disk);
    CFShow(disk);
    CFShow(keys);
    printf("DiskDescriptionChangedCallback dispatched\n");
    *(OSStatus *)context = 0;
    done = 1;
}

void DiskDescriptionRepairRunningCallback( DADiskRef disk, CFArrayRef keys, void *context )
{
    static bool repairRunning = false;
    static bool startedRepair = false;
    CFDictionaryRef description;
    
    CFRetain(disk);
    CFShow(disk);
    CFShow(keys);
    
    description = DADiskCopyDescription( disk );
    
    if ( description )
    {
        // Check the disk repair description update to make sure it is added or removed as expected.
        if ( CFArrayContainsValue( keys , CFRangeMake( 0, CFArrayGetCount( keys ) ) , kDADiskDescriptionRepairRunningKey ) )
        {
            if ( CFDictionaryGetValue( description, kDADiskDescriptionRepairRunningKey ) != NULL )
            {
                startedRepair = true;
                
                if ( !repairRunning )
                {
                    repairRunning = true;
                    printf("DiskDescriptionRepairRunningCallback dispatched - repair is in progress\n");
                }
            }
            else
            {
                if ( repairRunning )
                {
                    repairRunning = false;
                    printf("DiskDescriptionRepairRunningCallback dispatched - repair completed\n");
                }
                else
                {
                    printf("Removed repair in progress key, but repair was never started.\n");
                }
            }
        }
        
        // Make sure the repair was cleared after the mount ended.
        if ( CFArrayContainsValue( keys , CFRangeMake( 0, CFArrayGetCount( keys ) ) , kDADiskDescriptionVolumePathKey ) || done )
        {
            if ( startedRepair )
            {
                printf("Repair running key was%s cleared after mount.\n", ( repairRunning ) ? " not" : "");
            }
            else
            {
                printf("Disk repair did not run. Please use a disk that requires repair.\n");
            }
        }
    }
    else
    {
        printf("failed to copy description in repair description callback \n");
    }
}

void IdleCallback(void *context)
{
    printf("Idle received\n");
    *(OSStatus *)context = 0;
    done = 1;
}

bool WaitForCallback( void )
{
    bool cbdispatched = true;
    time_t start_t, end_t;
    start_t = time(NULL);
    end_t = start_t + 35;
    do {
        sleep(1);
        start_t = time(NULL);
        if ( start_t > end_t )
        {
            cbdispatched = false;
            break;
        }
    } while  ( !done );
    return cbdispatched;
}

pid_t pgrep(const char* proc_name)
{
    pid_t ret = 0;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pgrep -x %s", proc_name);
    FILE *f = popen(cmd, "r");
    if (!f) {
        printf("pgrep failed: %d (%s)", errno, strerror(errno));
        return 0;
    }
    char pid_str[256];
    if (fgets(pid_str, sizeof(pid_str), f)) {
        ret = (pid_t)strtoul(pid_str, NULL, 10);
    }
    pclose(f);
    return ret;
}

static void startTestTelemetry(void)
{
    if ( telemetryQueue == NULL )
    {
        telemetryQueue = dispatch_queue_create("com.example.DiskArbTestTelemetry", DISPATCH_QUEUE_SERIAL);
    }
    telemetryObserver = [AnalyticsEventObserver new];
    telemetryDelegate = [[DATestTelemetryObserverDelegate alloc]
                         initWithEvents:[NSSet setWithObject:DA_TELEMETRY_EVENT]];
    
    [telemetryObserver setEventObserverDelegate:telemetryDelegate queue:telemetryQueue];
    [telemetryObserver startObservingEventList:[[telemetryDelegate events] allObjects]
                              withErrorHandler:^(NSString * _Nonnull err) {
        NSLog(@"Failed to start observing event: %@\n", err);
    }];
}

static void stopTestTelemetry( CFStringRef fsTypeExpected )
{
    if ( telemetryObserver )
    {
        [telemetryObserver stopObserving];
    }
    
    if ( telemetryDelegate && [telemetryDelegate expectedEventCount] > 0 )
    {
        [telemetryDelegate verifyNumberOfEvents];
        
        if ( fsTypeExpected != NULL )
        {
            [telemetryDelegate verifyVolumeFSType:fsTypeExpected];
        }
    }
}

static int testMountAsync(struct clarg actargs[kDALast], bool approval)
{
    __block OSStatus        ret = 1;
    DASessionRef            _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    CFURLRef                mountpoint = NULL;
    CFStringRef             *mountoptions = NULL;
    bool                    trackRepairProgress = false;
    id                      subscriber;
    NSURL                   *deviceURL = NULL;
    DATestProgressObserver  *observer;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);

  
    if (!_disk)      {
        printf( "%s does not exist.\n", actargs[kDADevice].argument);
        goto exit;
    }
        
    if (DADiskCopyDescription(_disk) == NULL)
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( actargs[kDATestTelemetry].present )
    {
        startTestTelemetry();
    }
    
    if ( _session ) {
      
        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) actargs[kDAMountpath].argument, strlen( actargs[kDAMountpath].argument ), TRUE );
        if (actargs[kDAOptions].present)
        {
            mountoptions = calloc(2, sizeof(void *));
            mountoptions[0] = CFStringCreateWithCString(kCFAllocatorDefault, actargs[kDAOptions].argument, kCFStringEncodingUTF8);
        }
        int options = kDADiskUnmountOptionDefault;
            
        if (actargs[kDAWhole].present) {
            options |= kDADiskUnmountOptionWhole;
        }
        if (actargs[kDANoFollow].present) {
            options |= kDADiskMountOptionNoFollow;
        }
        
        bool useAuditToken = false;
        audit_token_t token = INVALID_AUDIT_TOKEN_VALUE;
        if ( actargs[kDAUseBlockCallback].present
               && actargs[kDAUseAuditToken].present
               && getCurrentAuditToken( &token ) == KERN_SUCCESS )
        {
            useAuditToken = true;
        }
        
        trackRepairProgress = actargs[kDARepairProgress].present;
        if ( trackRepairProgress )
        {
            NSString *deviceName = [NSString stringWithFormat:@"%s", actargs[kDADevice].argument];
            
            if ( deviceName )
            {
                if ( [deviceName hasPrefix:@"/dev/"] )
                {
                    deviceName = [deviceName substringFromIndex:@"/dev/".length];
                }
                
                deviceURL = [NSURL fileURLWithPath:[NSString stringWithFormat:@"/dev/r%@",
                                                        deviceName]];
                observer = [DATestProgressObserver new];
                observer.deviceURL = deviceURL;
                printf("Subscribing to progress updates for %s\n", deviceURL.path.UTF8String);
                [NSProgress addSubscriberForFileURL:deviceURL withPublishingHandler:^NSProgressUnpublishingHandler _Nullable(NSProgress * _Nonnull progress) {
                    observer.progress = progress;
                    NSProgressUnpublishingHandler unsubscribeHandler = ^{
                        printf( "Unsubscribed from updates for %s\n",
                                deviceURL.path.UTF8String );
                        ret = 0;
                    };
                    
                    return unsubscribeHandler;
                }];
            }
        }
        
        if ( approval == true )
        {
            
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskMountApprovalCallbackBlock bl = ^ DADissenterRef ( DADiskRef disk )
                {
                    ret = 0;
                    done = 1;
                    return NULL;
                };
                
                DARegisterDiskMountApprovalCallbackBlock( _session, NULL, bl);
                
                if ( useAuditToken )
                {
                    DADiskMountWithArgumentsAndBlockAndAuditToken( _disk, mountpoint, options, NULL, mountoptions , token );
                }
                else
                {
                    DADiskMountWithArgumentsAndBlock( _disk, mountpoint, options, NULL, mountoptions );
                }
            }
            else
            {
                DARegisterDiskMountApprovalCallback( _session, NULL, DiskApprovalCallback, NULL);
                DADiskMountWithArguments(_disk , mountpoint , options , NULL , &ret , mountoptions );
            }
        }
        else
        {
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskMountCallbackBlock block = ^( DADiskRef disk, DADissenterRef dissenter )
                {
                    DiskMountCallback( disk, dissenter, &ret);
                };
                
                if ( useAuditToken == true )
                {
                    DADiskMountWithArgumentsAndBlockAndAuditToken( _disk, mountpoint, options, block, mountoptions , token );
                }
                else
                {
                    DADiskMountWithArgumentsAndBlock( _disk, mountpoint, options, block, mountoptions );
                }
            }
            else
            {
                DADiskMountWithArguments( _disk, mountpoint, options, DiskMountCallback, &ret, mountoptions);
            }
        }
    }
    DASessionSetDispatchQueue(_session, myDispatchQueue);
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }
    
    if ( actargs[kDATestTelemetry].present )
    {
        CFDictionaryRef description = DADiskCopyDescription(_disk);
        stopTestTelemetry( CFDictionaryGetValue( description, kDADiskDescriptionVolumeKindKey ) );
    }

    if ( trackRepairProgress && ret == 0 )
    {
        ret = ( [observer verifyProgress] ) ? 0 : -1;
    }

exit:
    if (mountoptions) free(mountoptions);
    return ret;
}

static int testMount(struct clarg actargs[kDALast], bool approval)
{
    dispatch_async( dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT , 0 ) ,
                    ^{
        exit( testMountAsync( actargs , approval ) );
    });
    
    CFRunLoopRun();
    return -1; // Failed to run loop
}

static int testUnmount(struct clarg actargs[kDALast], int approval)
{
    __block OSStatus        ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
        
    if (DADiskCopyDescription(_disk) == NULL)
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( actargs[kDATestTelemetry].present )
    {
        startTestTelemetry();
        if ( actargs[kDAForce].present ) {
            [telemetryDelegate setUnmountForced:true];
        }
    }
    
    if ( _session ) {
        
        int options = kDADiskUnmountOptionDefault;
        if (actargs[kDAForce].present) {
            options |= kDADiskUnmountOptionForce;
        }
        
        if (actargs[kDAWhole].present) {
            options |= kDADiskUnmountOptionWhole;
        }
        
        bool useAuditToken = false;
        audit_token_t token = INVALID_AUDIT_TOKEN_VALUE;
        if ( actargs[kDAUseBlockCallback].present
               && actargs[kDAUseAuditToken].present
               && getCurrentAuditToken( &token ) == KERN_SUCCESS )
        {
            useAuditToken = true;
        }
        
        if ( approval == true )
        {
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskUnmountApprovalCallbackBlock bl = ^ DADissenterRef ( DADiskRef disk )
                {
                    ret = 0;
                    done = 1;
                    return NULL;
                };
                
                DARegisterDiskUnmountApprovalCallbackBlock( _session, NULL, bl);
                
                if ( useAuditToken == true )
                {
                    DADiskUnmountWithBlockAndAuditToken( _disk, options, NULL, token );
                }
                else
                {
                    DADiskUnmountWithBlock( _disk, options, NULL );
                }
            }
            else
            {
                DARegisterDiskUnmountApprovalCallback( _session, NULL, DiskApprovalCallback, &ret);
                DADiskUnmount( _disk, options, NULL,  &ret );
            }
        }
        else
        {
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskUnmountCallbackBlock block = ^( DADiskRef disk, DADissenterRef dissenter  )
                {
                    DiskUnmountCallback( disk, dissenter, &ret );
                };
                
                if ( useAuditToken == true )
                {
                    DADiskUnmountWithBlockAndAuditToken( _disk, options, block, token );
                }
                else
                {
                    DADiskUnmountWithBlock( _disk, options, block );
                }
            }
            else
            {
                DADiskUnmount( _disk, options, DiskUnmountCallback, &ret );
            }
        }
    }
    DASessionSetDispatchQueue(_session, myDispatchQueue);
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

    if ( actargs[kDATestTelemetry].present )
    {
        CFDictionaryRef description = DADiskCopyDescription(_disk);
        stopTestTelemetry( CFDictionaryGetValue( description, kDADiskDescriptionVolumeKindKey ) );
    }
    
exit:
    return ret;
}

static int testEject(struct clarg actargs[kDALast], int approval)
{
    __block OSStatus        ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    CFDictionaryRef     description = NULL;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
        
    description = DADiskCopyDescription(_disk);
    if (description) {
            
        if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaWholeKey ) == NULL ||
            CFDictionaryGetValue( description, kDADiskDescriptionMediaWholeKey ) == kCFBooleanFalse)
        {
            printf( "%s is not a whole device.\n ", actargs[kDADevice].argument);
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( actargs[kDATestTelemetry].present )
    {
        startTestTelemetry();
    }
    
    if ( _session ) {
        bool useAuditToken = false;
        audit_token_t token = INVALID_AUDIT_TOKEN_VALUE;
        if ( actargs[kDAUseBlockCallback].present
               && actargs[kDAUseAuditToken].present
               && getCurrentAuditToken( &token ) == KERN_SUCCESS )
        {
            useAuditToken = true;
        }
        
        int options = kDADiskUnmountOptionDefault;
        if (approval == true)
        {
            
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskEjectApprovalCallbackBlock bl = ^ DADissenterRef ( DADiskRef disk )
                {
                    ret = 0;
                    done = 1;
                    return NULL;
                };
                
                DARegisterDiskEjectApprovalCallbackBlock( _session, NULL, bl);
                
                if ( useAuditToken )
                {
                    DADiskEjectWithBlockAndAuditToken( _disk,
                                                       options,
                                                       NULL,
                                                       token );
                }
                else
                {
                    DADiskEjectWithBlock( _disk,
                                         options,
                                         NULL );
                }
            }
            else
            {
                DARegisterDiskEjectApprovalCallback( _session, NULL, DiskApprovalCallback, &ret);
                DADiskEject( _disk,
                            options,
                            NULL,
                            &ret );
            }
        }
        else
        {
            if ( actargs[kDAUseBlockCallback].present )
            {
                DADiskEjectCallbackBlock block = ^( DADiskRef disk, DADissenterRef dissenter )
                {
                    DiskEjectCallback( disk, dissenter, &ret);
                };
                
                if ( useAuditToken )
                {
                    DADiskEjectWithBlockAndAuditToken( _disk,
                                                       options,
                                                       block,
                                                       token );
                }
                else
                {
                    DADiskEjectWithBlock( _disk,
                                         options,
                                         block);
                }
            }
            else
            {
                DADiskEject( _disk, options, DiskEjectCallback, &ret );
            }
            
        }
        
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

    if ( actargs[kDATestTelemetry].present )
    {
        stopTestTelemetry( NULL );
    }
    
exit:
    return ret;
}


static int testDiskAppeared(struct clarg actargs[kDALast])
{
    __block int  ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        if ( actargs[kDAUseBlockCallback].present )
        {
            DADiskAppearedCallbackBlock block =  ^( DADiskRef disk )
            {
                DiskAppearedCallback( disk, &ret);
            };
            DARegisterDiskAppearedCallbackBlock(_session, NULL, block );
        }
       else
       {
           DARegisterDiskAppearedCallback(_session, NULL, DiskAppearedCallback, &ret);
       }
    
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

exit:
    return ret;
}

static int testDiskDisAppeared(struct clarg actargs[kDALast])
{
    __block int  ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    CFDictionaryRef     description = NULL;
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
    
    description = DADiskCopyDescription(_disk);
    if (description) {
            
        if ( CFDictionaryGetValue( description, kDADiskDescriptionMediaWholeKey ) == NULL ||
            CFDictionaryGetValue( description, kDADiskDescriptionMediaWholeKey ) == kCFBooleanFalse)
        {
            printf( "%s is not a whole device.\n ", actargs[kDADevice].argument);
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    if ( _session ) {

        if ( actargs[kDAUseBlockCallback].present )
        {
            DADiskDisappearedCallbackBlock block =  ^( DADiskRef disk )
            {
                DiskDisAppearedCallback( disk, &ret);
            };
            DARegisterDiskDisappearedCallbackBlock(_session, NULL, block );
        }
        else
        {
            DARegisterDiskDisappearedCallback(_session, NULL, DiskDisAppearedCallback, &ret);
        }
        DADiskEject( _disk,
                     kDADiskUnmountOptionDefault,
                     NULL,
                     NULL );
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
        printf( "Eject failed. Check if any of the volumes are mounted.\n ");
    }

exit:
    return ret;
}


static int testDiskDescriptionChanged(struct clarg actargs[kDALast])
{
    __block int         ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    CFDictionaryRef     description = NULL;
    bool         checkRepairStatus = actargs[kDARepairRunning].present;
    DADiskDescriptionChangedCallback descCallback;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
    
    description = DADiskCopyDescription(_disk);
    if (description) {
            
        if ( !checkRepairStatus && CFDictionaryGetValue(description, kDADiskDescriptionVolumePathKey) == NULL )
        {
            printf( "volume is not mounted. mount the volume and try again.\n ");
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    descCallback = ( checkRepairStatus ) ? DiskDescriptionRepairRunningCallback : DiskDescriptionChangedCallback;
    
    if ( _session ) {

        if ( actargs[kDAUseBlockCallback].present )
        {
            DADiskDescriptionChangedCallbackBlock block =  ^( DADiskRef disk, CFArrayRef keys )
            {
                descCallback( disk, keys, (void *) &ret);
            };
            DARegisterDiskDescriptionChangedCallbackBlock(_session, NULL, NULL, block );
        }
        else
        {
            DARegisterDiskDescriptionChangedCallback(_session, NULL, NULL, descCallback, &ret);
        }

        if ( checkRepairStatus )
        {
            DADiskMountWithArguments ( _disk ,
                                       NULL ,
                                       kDADiskMountOptionDefault ,
                                       DiskMountCallback ,
                                       &ret ,
                                       NULL );
        }
        else {
            DADiskUnmount( _disk,
                          kDADiskUnmountOptionDefault,
                          NULL,
                          NULL );
        }
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

exit:
    return ret;
}

static int testDASetFSKitAdditions(struct clarg actargs[kDALast])
{
#ifdef DA_FSKIT
    __block int              ret = 1;
    DASessionRef             _session = DASessionCreate(kCFAllocatorDefault);
    int                      validArgs[] = {kDADevice};
    CFDictionaryRef          description = NULL;
    __block DADiskRef        retrievedDisk = NULL;
    NSString                *aString = @"AddedStringsAreWonderful";

    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);

    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }

    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);

    if ( _session ) {

        DADiskDescriptionChangedCallbackBlock block =  ^( DADiskRef disk, CFArrayRef keys )
        {
            CFRetain( disk );
            retrievedDisk = disk;
            done = 1;
        };
        DARegisterDiskDescriptionChangedCallbackBlock(_session, NULL, NULL, block );

        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    else
    {
        goto exit;
    }

    DADiskSetFSKitAdditions( _disk, (__bridge CFDictionaryRef)@{@"FSTestKey":aString}, ^(DAReturn error) {
        // We could do some coordinated wait w.r.t. this block running and 'block'. But we really want to test
        // that a disk gets updated, so just test for that.
        if (error)
        {
            printf( "DADiskSetFSKitAdditions failed, error %d", error );
            done = 1;
            ret = -1; // Fall through the !retrievedDisk test
        }
    });
    if ( WaitForCallback() == false )
    {
        ret = -1;
        goto exit;
    }

    if ( !retrievedDisk )
    {
        ret = -1;
        goto exit;
    }

    // We got a disk, get its description and forget the disk.
    description = DADiskCopyDescription( retrievedDisk );
    CFRelease( retrievedDisk );
    retrievedDisk = NULL;

    // Make sure the description includes the right key/value
    if ( ![ aString isEqualToString: (__bridge NSString *)CFDictionaryGetValue( description, CFSTR("FSTestKey") ) ] )
    {
        printf( "Disk description didn't contain FSTestKey : %s\n", aString.description.UTF8String );
        CFShow( description );
        ret = -1;
        goto exit;
    }

    // Now set up to try again but w/o any addition
    CFRelease( description );
    description = NULL;
    ret = 0;
    done = 0;

    DADiskSetFSKitAdditions( _disk, NULL, ^(DAReturn error) {
        // We could do some coordinated wait w.r.t. this block running and 'block'. But we really want to test
        // that a disk gets updated, so just test for that.
        if (error)
        {
            printf( "DADiskSetFSKitAdditions failed clearing dict, error %d", error );
            ret = -1; // Fall through the !retrievedDisk test
        }
    });
    // Remember the session still has the disk change callback set above
    if ( WaitForCallback() == false )
    {
        ret = -1;
        goto exit;
    }

    if ( !retrievedDisk )
    {
        ret = -1;
        goto exit;
    }

    // We got a disk, get its description and forget the disk.
    description = DADiskCopyDescription( retrievedDisk );
    CFRelease( retrievedDisk );
    retrievedDisk = NULL;

    // Make sure the description does NOT include the right key
    if ( CFDictionaryGetValue( description, CFSTR("FSTestKey") ) )
    {
        NSObject *whatsThere = (__bridge NSObject *) CFDictionaryGetValue( description, CFSTR("FSTestKey") );
        printf( "Disk description contained FSTestKey : %s\n", whatsThere.description.UTF8String );
        CFShow( description );
        ret = -1;
        goto exit;
    }
    CFRelease( description );

exit:
    return ret;
#else
    printf("FSKit not supported, skipping test\n");
    return 0;
#endif
}

static int testDAIdle(struct clarg actargs[kDALast])
{
    __block int             ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
  
    DAIdleCallbackBlock block;
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        if ( actargs[kDAUseBlockCallback].present )
        {
            DAIdleCallbackBlock block =  ^( void )
            {
                IdleCallback( &ret );
            };
            DARegisterIdleCallbackWithBlock(_session, block );
        }
        else
        {
            DARegisterIdleCallback( _session, IdleCallback, &ret );
        }
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

exit:
    if (ret == 0)
    {
        DAUnregisterCallback(_session, (void *) block, NULL);
    }
    
    return ret;
}

static int testRename(struct clarg actargs[kDALast])
{
    __block OSStatus        ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice, kDAName};
    CFDictionaryRef     description = NULL;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
        
    description = DADiskCopyDescription(_disk);
    if (description) {
            
        if ( CFDictionaryGetValue(description, kDADiskDescriptionVolumePathKey) == NULL )
        {
            printf( "volume is not mounted. mount the volume and try again.\n ");
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {
        bool useAuditToken = false;
        audit_token_t token = INVALID_AUDIT_TOKEN_VALUE;
        if ( actargs[kDAUseBlockCallback].present
               && actargs[kDAUseAuditToken].present
               && getCurrentAuditToken( &token ) == KERN_SUCCESS )
        {
            useAuditToken = true;
        }
        
        CFStringRef name = CFStringCreateWithCString(kCFAllocatorDefault,actargs[kDAName].argument, kCFStringEncodingUTF8);
        if ( actargs[kDAUseBlockCallback].present )
        {
            DADiskRenameCallbackBlock block =  ^( DADiskRef disk, DADissenterRef dissenter )
            {
                DiskRenameCallback( disk, dissenter, &ret );
            };
            if ( useAuditToken == true )
            {
                DADiskRenameWithBlockAndAuditToken(_disk, name, 0, block, token );
            }
            else
            {
                DADiskRenameWithBlock(_disk, name, 0, block );
            }
        }
        else
        {
            DADiskRename(_disk, name, 0, DiskRenameCallback, &ret);
        }
        
        DASessionSetDispatchQueue(_session, myDispatchQueue);
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
    }

exit:
    return ret;
}

static int testEncode(struct clarg actargs[kDALast])
{
    OSStatus                     ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice, kDAValue};
    CFDictionaryRef     description = NULL;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if (!_disk)      {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
        
    description = DADiskCopyDescription(_disk);
    if (description) {
            
        if ( CFDictionaryGetValue(description, kDADiskDescriptionVolumePathKey) == NULL )
        {
            printf( "volume is not mounted. mount the volume and try again.\n ");
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    char* endptr;
    int value = strtol(actargs[kDAValue].argument, &endptr, 10);
    printf( "Encoding with value %d\n" , value);
    if ( _session ) {
          ret = _DADiskSetEncoding(_disk, value);
    }
    printf( "Setting Encoding completed with status %X \n" , ret);

exit:
    return ret;
}

static int TerminateDaemonToTriggerRelaunch()
{
    pid_t pid = 0;
    int status;
    char command[256];

    pid = pgrep("diskarbitrationd");
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
#if !TARGET_OS_OSX
    sleep(1);
#endif
    return 0;
}

static int testDASessionKeepAliveWithDAIdle(struct clarg actargs[kDALast])
{
    int             ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
  
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        DARegisterIdleCallback(_session, IdleCallback, &ret);
        DASessionSetDispatchQueue(_session, myDispatchQueue);
        DASessionKeepAlive( _session, myDispatchQueue);
        printf ("set the current session to be kept alive across daemon launches\n");
    }
    
    if ( WaitForCallback() == false )
    {
        ret = -1;
        goto exit;
    }
    
    // wait for DA to exit
    if ((ret = TerminateDaemonToTriggerRelaunch()) != 0)
    {
        goto exit;
    }
    
#if !TARGET_OS_OSX
    printf ("Attach an external drive or run datest from another terminal\n");
#endif
    done = 0;
    if ( WaitForCallback() == false )
    {
        printf ("Failed to receive callbacks from diskarbitrationd\n");
        ret = -1;
    }
exit:
    return ret;
}

static int testDASessionKeepAliveWithDADiskAppeared(struct clarg actargs[kDALast])
{
    int             ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
  
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        DARegisterDiskAppearedCallback(_session, NULL, DiskAppearedCallback, &ret);
        DASessionSetDispatchQueue(_session, myDispatchQueue);
        DASessionKeepAlive( _session, myDispatchQueue);
        printf ("set the current session to be kept alive across daemon launches\n");
        ret = 0;
    }
    
    if ( ret == 0 )
    {
        if ( WaitForCallback() == false )
        {
            ret = -1;
            goto exit;
        }
    }
 
    // wait for DA to exit
    if ((ret = TerminateDaemonToTriggerRelaunch()) != 0)
    {
        goto exit;
    }
#if !TARGET_OS_OSX
    printf ("Attach an external drive or run datest from another terminal\n");
#endif
    done = 0;
    if ( WaitForCallback() == false )
    {
        printf ("Failed to receive callbacks from diskarbitrationd\n");
        ret = -1;
    }
exit:
    return ret;
}

static int testDASessionKeepAliveWithDADiskDescriptionChanged(struct clarg actargs[kDALast])
{
    int             ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
  
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        DARegisterDiskDescriptionChangedCallback(_session, NULL, NULL, DiskDescriptionChangedCallback, &ret);
        DASessionSetDispatchQueue(_session, myDispatchQueue);
        DASessionKeepAlive( _session, myDispatchQueue);
        printf ("set the current session to be kept alive across daemon launches\n");
        ret = 0;
    }
 
    // wait for DA to exit
    if ((ret = TerminateDaemonToTriggerRelaunch()) != 0)
    {
        goto exit;
    }

    printf ("Attach an external drive or run datest --mount from another terminal\n");

    done = 0;

    if ( WaitForCallback() == false )
    {
        printf ("Failed to receive callbacks from diskarbitrationd\n");
        ret = -1;
    }
exit:
    return ret;
}

static int testDASessionKeepAliveWithDARegisterDiskAppeared(struct clarg actargs[kDALast])
{
    int             ret = 1;
    DASessionRef _session = DASessionCreate(kCFAllocatorDefault);
  
        
    myDispatchQueue = dispatch_queue_create("com.example.DiskArbTest", DISPATCH_QUEUE_SERIAL);
    
    if ( _session ) {

        DASessionSetDispatchQueue(_session, myDispatchQueue);
        DASessionKeepAlive( _session, myDispatchQueue);
        printf ("set the current session to be kept alive across daemon launches\n");
        ret = 0;
    }
 
    // wait for DA to exit
    if ((ret = TerminateDaemonToTriggerRelaunch()) != 0)
    {
        goto exit;
    }
    
    DARegisterDiskAppearedCallback(_session, NULL, DiskAppearedCallback, &ret);
    done = 0;
    if ( WaitForCallback() == false )
    {
        printf ("Failed to receive callbacks from diskarbitrationd\n");
        ret = -1;
    }
exit:
    return ret;
}

static int testDASetDiskAdoption(struct clarg actargs[kDALast])
{
    __block OSStatus        ret = 1;
    DASessionRef            _session = DASessionCreate(kCFAllocatorDefault);
    int                     validArgs[] = {kDADevice};
    CFDictionaryRef         description = NULL;
    bool                    adoption = false;
    
    if (validateArguments(validArgs, sizeof(validArgs)/sizeof(int), actargs))
    {
        goto exit;
    }
    
    if ( strnlen( actargs[kDASetDiskAdoption].argument ,
                  sizeof(actargs[kDASetDiskAdoption].argument ) ) != 1 )
    {
        usage();
        goto exit;
    }
    
    switch (actargs[kDASetDiskAdoption].argument[0])
    {
        case 'y':
            adoption = true;
            break;
        case 'n':
            adoption = false;
            break;
        default:
            usage();
            goto exit;
    }
    
    DADiskRef _disk = DADiskCreateFromBSDName(kCFAllocatorDefault, _session, actargs[kDADevice].argument);
  
    if ( !_disk )
    {
        printf( "%s does not exist", actargs[kDADevice].argument);
        goto exit;
    }
        
    description = DADiskCopyDescription(_disk);
    if ( description )
    {
            
        if ( CFDictionaryGetValue(description, kDADiskDescriptionVolumePathKey) == NULL )
        {
            printf( "volume is not mounted. mount the volume and try again.\n ");
            goto exit;
        }
    }
    else
    {
        printf( "DADiskCopyDescription failed.\n ");
        goto exit;
    }
    
    if ( _session )
    {
        if ( @available(macOS 16.0, iOS 19.0, *) )
        {
            ret = _DADiskSetAdoption(_disk, adoption);
            printf( "Setting Disk Adoption to %d completed with status %X \n" , adoption , ret );
        }
        else
        {
            printf( "_DADiskSetAdoption is not available on this platform.\n" );
        }
    }

exit:
    return ret;
}

int main (int argc, char * argv[])
{

    int ch, longindex;

    setlinebuf(stdout);
    
    if(argc == 1) {
        usage();
    }
    
    
    while ((ch = getopt_long_only(argc, argv, "", opts, &longindex)) != -1) {
        
        switch(ch) {
            case kDAHelp:
                usage();
                break;
            case '?':
            case ':':
                usage();
                break;
            default:
                // common handling for all other options
            {
                struct option *opt = &opts[longindex];
                
                if(actargs[ch].present) {
                    warnx("Option \"%s\" already specified", opt->name);
                    usage();
                    break;
                } else {
                    actargs[ch].present = 1;
                }
                
                switch(opt->has_arg) {
                    case no_argument:
                        actargs[ch].hasArg = 0;
                        break;
                    case required_argument:
                        actargs[ch].hasArg = 1;
                        strlcpy(actargs[ch].argument, optarg, sizeof(actargs[ch].argument));
                        break;
                    case optional_argument:
                        if(argv[optind] && argv[optind][0] != '-') {
                            actargs[ch].hasArg = 1;
                            strlcpy(actargs[ch].argument, argv[optind], sizeof(actargs[ch].argument));
                        } else {
                            actargs[ch].hasArg = 0;
                        }
                        break;
                }
            }
                break;
        }
    }

    argc -= optind;
    argc += optind;
    
    if(actargs[kDAMount].present) {
        return testMount(actargs, 0);
    }
    
    if(actargs[kDAMountApproval].present) {
        return testMount(actargs, 1);
    }

    if(actargs[kDAUnmount].present) {
        return testUnmount(actargs, 0);
    }
    
    if(actargs[kDAUnmountApproval].present) {
        return testUnmount(actargs, 1);
    }

    if(actargs[kDAEject].present) {
        return testEject(actargs, 0);
    }
       
    if(actargs[kDAEjectApproval].present) {
        return testEject(actargs, 1);
    }
       
    if(actargs[kDARename].present) {
        return testRename(actargs);
    }
    
    if(actargs[kDAEncode].present) {
        return testEncode(actargs);
    }
    
    if(actargs[kDADiskAppeared].present) {
        return testDiskAppeared(actargs);
    }

    if(actargs[kDADiskDisAppeared].present) {
        return testDiskDisAppeared(actargs);
    }
    
    if(actargs[kDADiskDescChanged].present) {
        return testDiskDescriptionChanged(actargs);
    }
    
    if(actargs[kDAIdle].present) {
        return testDAIdle(actargs);
    }
    
    if(actargs[kDASessionKeepAliveWithDAIdle].present) {
        return testDASessionKeepAliveWithDAIdle(actargs);
    }
    if(actargs[kDASessionKeepAliveWithDADiskAppeared].present) {
        return testDASessionKeepAliveWithDADiskAppeared(actargs);
    }
    if(actargs[kDASessionKeepAliveWithDARegisterDiskAppeared].present) {
        return testDASessionKeepAliveWithDARegisterDiskAppeared(actargs);
    }
    if(actargs[kDASessionKeepAliveWithDADiskDescriptionChanged].present) {
        return testDASessionKeepAliveWithDADiskDescriptionChanged(actargs);
    }

    if(actargs[kDASetFSKitAdditions].present) {
        return testDASetFSKitAdditions(actargs);
    }
    
    if(actargs[kDASetDiskAdoption].present) {
        return testDASetDiskAdoption(actargs);
    }

    /* default */
    usage();
    return 1;
}
