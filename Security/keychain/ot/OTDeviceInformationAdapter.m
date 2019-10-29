#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/CKKS.h"

#include <SystemConfiguration/SystemConfiguration.h>
#import <Security/SecInternalReleasePriv.h>

#if TARGET_OS_OSX
#include <sys/sysctl.h>
#include <IOKit/IOKitLib.h>
#else
#import <sys/utsname.h>
#include <MobileGestalt.h>
#endif

static void updateDeviceNameChanges(SCDynamicStoreRef store, CFArrayRef keys, void *context);

@interface OTDeviceInformationActualAdapter ()
@property CKKSListenerCollection<id<OTDeviceInformationNameUpdateListener>>* deviceNameUpdateListeners;
@property (assign) SCDynamicStoreRef store;
@end

@implementation OTDeviceInformationActualAdapter

- (void)dealloc {
    if (self.store) {
        CFRelease(self.store);
        self.store = NULL;
    }
}

- (NSString*)modelID
{
    static NSString *hwModel = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if TARGET_OS_SIMULATOR
        // Asking for a real value in the simulator gives the results for the underlying mac. Not particularly useful.
        hwModel = [NSString stringWithFormat:@"%s", getenv("SIMULATOR_MODEL_IDENTIFIER")];
#elif TARGET_OS_OSX
        size_t size;
        sysctlbyname("hw.model", NULL, &size, NULL, 0);
        char *sysctlString = malloc(size);
        sysctlbyname("hw.model", sysctlString, &size, NULL, 0);
        hwModel = [[NSString alloc] initWithUTF8String:sysctlString];
        free(sysctlString);

        // macOS running virtualized sometimes has new and unknown model IDs.
        // So, if we don't recognize the model ID, return something more useful.
        if(!([hwModel hasPrefix:@"iMac"] ||
             [hwModel hasPrefix:@"Mac"])) {
            hwModel = [NSString stringWithFormat:@"MacUnknown-%@", hwModel];
        }
#else
        struct utsname systemInfo;
        uname(&systemInfo);

        hwModel = [NSString stringWithCString:systemInfo.machine
                                     encoding:NSUTF8StringEncoding];
#endif
    });
    return hwModel;
}

- (NSString* _Nullable)deviceName
{
    if (SecIsInternalRelease()) {
        NSString *deviceName = CFBridgingRelease(SCDynamicStoreCopyComputerName(NULL, NULL));
        return deviceName;
    } else {
        return nil;
    }
}

- (void)registerForDeviceNameUpdates:(id<OTDeviceInformationNameUpdateListener>)listener
{
    // Octagon only uses the device name on internal releases.
    // Therefore, if this is not an internal release, don't bother registering clients--they don't need the update.
    if (SecIsInternalRelease()) {
        @synchronized (self) {
            [self setupDeviceNameListener];
            [self.deviceNameUpdateListeners registerListener:listener];
        }
    }
}

- (NSString*)osVersion
{
    return SecCKKSHostOSVersion();
}

#if TARGET_OS_IPHONE

#include <MobileGestalt.h>

- (NSString*)serialNumber
{
    int mgError = kMGNoError;
    NSString *serialNumber = CFBridgingRelease(MGCopyAnswerWithError(kMGQSerialNumber, NULL, &mgError));
    if (![serialNumber isKindOfClass:[NSString class]]) {
        serialNumber = nil;
        secnotice("octagon", "failed getting serial number: %d", mgError);
    }
    return serialNumber;
}

#else

- (NSString*)serialNumber
{
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert == MACH_PORT_NULL) {
        secnotice("octagon", "failed getting serial number (platform IOPlatformExpertDevice)");
        return nil;
    }
    NSString *serialNumber = CFBridgingRelease(IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0));
    if (![serialNumber isKindOfClass:[NSString class]]) {
        serialNumber = nil;
        secnotice("octagon", "failed getting serial number (IORegistryEntry)");
    }
    IOObjectRelease(platformExpert);
    return serialNumber;
}

#endif


- (void)setupDeviceNameListener {
    if (self.deviceNameUpdateListeners == nil) {
        self.deviceNameUpdateListeners = [[CKKSListenerCollection alloc] initWithName:@"OTDeviceInformationActualAdapter"];

        CFStringRef computerKey = SCDynamicStoreKeyCreateComputerName(NULL);
        if (computerKey == NULL) {
            return;
        }
        NSArray *keys = @[ (__bridge NSString *)computerKey];
        CFRelease(computerKey);

        SCDynamicStoreContext context = { .info = (void *)(__bridge CFTypeRef)self };
        self.store = SCDynamicStoreCreate(NULL, CFSTR("OTDeviceInformationActualAdapter"), updateDeviceNameChanges, &context);
        if (self.store == NULL) {
            return;
        }

        SCDynamicStoreSetNotificationKeys(self.store, (__bridge CFArrayRef)keys, NULL);
        SCDynamicStoreSetDispatchQueue(self.store, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    }
}

@end

static void updateDeviceNameChanges(SCDynamicStoreRef store, CFArrayRef keys, void *info)
{
    secnotice("octagon", "Notified that the device name has changed");
    OTDeviceInformationActualAdapter *adapter = (__bridge id)info;

    [adapter.deviceNameUpdateListeners iterateListeners:^void(id<OTDeviceInformationNameUpdateListener> object){
        [object deviceNameUpdated];
    }];
}

