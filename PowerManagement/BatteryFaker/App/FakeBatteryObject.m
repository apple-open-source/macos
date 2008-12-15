#include <IOKit/pwr_mgt/IOPM.h>
#import "FakeBatteryObject.h"

#define kGoodConf   @"Good Confidence"
#define kFairConf   @"Fair Confidence"
#define kPoorConf   @"Poor Confidence"

#define kGoodHealth   @"Good Health"
#define kFairHealth   @"Fair Health"
#define kPoorHealth   @"Poor Health"


@implementation FakeBatteryObject

- (id)init
{
    if(![super init]) {
        return nil;
    }
    
    properties = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)free
{
    [properties release];
}


- (void)setPropertiesAndUpdate:(NSDictionary *)newProperties
{
    IOReturn ret = kIOReturnSuccess;
    io_registry_entry_t faker_kext_registry_entry;


    if (!newProperties) return;
    
    /* Merge properties in newProperties into properties.
       Do not overwrite proprties dictionaly altgother.
     */

    // Stuff the resulting properties down to the kext.
    
    faker_kext_registry_entry = IOServiceGetMatchingService(
                                MACH_PORT_NULL,
                                IOServiceMatching("BatteryFaker"));
    
    if(!faker_kext_registry_entry) {
        goto exit;
    }
    ret = IORegistryEntrySetCFProperty( faker_kext_registry_entry,
                                CFSTR("Battery Properties"), 
                                (CFTypeRef) newProperties );

exit:
    if(MACH_PORT_NULL != faker_kext_registry_entry) {
        IOObjectRelease(faker_kext_registry_entry);
    }
}

- (NSDictionary *)properties
{
    if(!properties) {
        return [NSDictionary dictionary];
    }
    
    return properties;
}

@end
