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

- (void)awake
{
    int image_index = [BattPercentageSlider intValue]/10;
    int is_charging = [BattChargingCheck intValue];
    
    [BattStatusImage setImage:
        [owner batteryImage:image_index isCharging:is_charging]];
}

- (void)UIchange
{
    IOReturn ret = kIOReturnSuccess;
    io_registry_entry_t faker_kext_registry_entry;
    int image_index = [BattPercentageSlider intValue]/10;
    int is_charging = [BattChargingCheck intValue];
    int full_charge_capacity = 0;
    
    NSNumber    *numtrue = [NSNumber numberWithBool:true];
    NSNumber    *numfalse = [NSNumber numberWithBool:false];
    
    [BattStatusImage setImage:
        [owner batteryImage:image_index isCharging:is_charging]];

    [properties removeAllObjects];

    // AC Connected

    [properties setObject:([ACPresentCheck intValue] ? numtrue : numfalse) 
                forKey:@kIOPMPSExternalConnectedKey];

    // Batt Present

    [properties setObject:([BattPresentCheck intValue] ? numtrue : numfalse)
                forKey:@kIOPMPSBatteryInstalledKey];

    // Is Charging

    [properties setObject:([BattChargingCheck intValue] ? numtrue : numfalse)
                forKey:@kIOPMPSIsChargingKey];

    // Capacity

    [properties setObject:[NSNumber numberWithInt:[DesignCapCell intValue]] 
                forKey:@"Design Capacity"];

    full_charge_capacity = [MaxCapCell intValue];
    [properties setObject:[NSNumber numberWithInt: full_charge_capacity ] 
                forKey:@kIOPMPSMaxCapacityKey];

    [properties setObject:[NSNumber numberWithInt:
                (([BattPercentageSlider intValue] * full_charge_capacity)/100)]
                forKey:@kIOPMPSCurrentCapacityKey];

    // Amperage

    [properties setObject:[NSNumber numberWithInt:[AmpsCell intValue]] 
                forKey:@kIOPMPSAmperageKey];

    // Voltage

    [properties setObject:[NSNumber numberWithInt:[VoltsCell intValue]] 
                forKey:@kIOPMPSVoltageKey];

    // Cycle Count

    [properties setObject:[NSNumber numberWithInt:[CycleCountCell intValue]] 
                forKey:@kIOPMPSCycleCountKey];
                
    // Max Err    

    [properties setObject:[NSNumber numberWithInt:[MaxErrCell intValue]] 
                forKey:@"MaxErr"];

    // Health

    int     health_int = 0;
    NSString    *health_str = [HealthMenu stringValue];
    if([health_str isEqualTo:kGoodHealth]) {
        health_int = kIOPMGoodValue;
    } else if ([health_str isEqualTo:kFairHealth]) {
        health_int = kIOPMFairValue;
    } else if ([health_str isEqualTo:kPoorHealth]) {
        health_int = kIOPMPoorValue;
    }


    [properties setObject:[NSNumber numberWithInt : health_int]
                forKey:@kIOPMPSBatteryHealthKey];


    // Confidence
    
    int     confidence_int = 0;
    NSString    *conf_str = [ConfidenceMenu stringValue];
    if([conf_str isEqualTo:kGoodConf]) {
        confidence_int = kIOPMGoodValue;
    } else if ([conf_str isEqualTo:kFairConf]) {
        confidence_int = kIOPMFairValue;
    } else if ([conf_str isEqualTo:kPoorConf]) {
        confidence_int = kIOPMPoorValue;
    }
    [properties setObject: [NSNumber numberWithInt : confidence_int]
                forKey:@kIOPMPSHealthConfidenceKey];


    // Stuff the resulting properties down to the kext.
    
    faker_kext_registry_entry = IOServiceGetMatchingService(
                                MACH_PORT_NULL,
                                IOServiceMatching("BatteryFaker"));
    
    if(!faker_kext_registry_entry) {
        goto exit;
    }
    
    ret = IORegistryEntrySetCFProperty( faker_kext_registry_entry,
                                CFSTR("Battery Properties"), 
                                (CFTypeRef) properties );

exit:
    if(MACH_PORT_NULL != faker_kext_registry_entry) {
        IOObjectRelease(faker_kext_registry_entry);
    }
    
    return;
}

- (NSDictionary *)properties
{
    if(!properties) {
        return [NSDictionary dictionary];
    }
    
    return properties;
}

@end
