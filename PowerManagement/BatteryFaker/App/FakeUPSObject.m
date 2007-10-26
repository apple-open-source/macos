#import<SystemConfiguration/SystemConfiguration.h>
#import<SystemConfiguration/SCPrivate.h>
#import <IOKit/ps/IOPSKeys.h>
#import "FakeUPSObject.h"
#import "IOUPSPlugin.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#define FakeLog NSLog

#define     kUPSBootstrapServerName     "com.apple.FakeUPS.control"

bool IOUPSMIGServerIsRunning(
    mach_port_t * bootstrap_port_ref, 
    mach_port_t * upsd_port_ref)
{
    boolean_t active = FALSE;
    Boolean result = false;
    kern_return_t kern_result = KERN_SUCCESS;
    mach_port_t   bootstrap_port;

    if (bootstrap_port_ref && (*bootstrap_port_ref != MACH_PORT_NULL)) {
        bootstrap_port = *bootstrap_port_ref;
    } else {
        /* Get the bootstrap server port */
        kern_result = task_get_bootstrap_port(
                            mach_task_self(), &bootstrap_port);
        if (kern_result != KERN_SUCCESS) {
            result = false;
            goto finish;
        }
        if (bootstrap_port_ref) {
            *bootstrap_port_ref = bootstrap_port;
        }
    }

    /* Check server status */
    kern_result = bootstrap_status( bootstrap_port, 
                        kUPSBootstrapServerName, &active);

    if (BOOTSTRAP_SUCCESS == kern_result)
    {
        if (active) {
            result = true;

            if (upsd_port_ref)
            {
                bootstrap_look_up( bootstrap_port, 
                            kUPSBootstrapServerName, upsd_port_ref);
            }
            goto finish;
        }
    } else if (BOOTSTRAP_UNKNOWN_SERVICE == kern_result)
    {
        result = false;
        goto finish;
    }

finish:
    return result;
}


@implementation FakeUPSObject

- (id)init
{
    if(!(self = [super init])) {
        return nil;
    }
    
    _UPSPlugInLoaded = false;

    return self;
}

- (void)free
{
    [properties release];
}

- (NSDictionary *)properties
{
    if(!properties) {
        return [NSDictionary dictionary];
    }
    
    return properties;
}

- (void)UIchange
{
    int full_charge_capacity = 0;
    NSNumber    *tmp;

    int image_index                 = [PercentageSlider intValue]/10;
    int is_charging                 = [ChargingCheck intValue];

    NSNumber    *numtrue = [NSNumber numberWithBool:true];
    NSNumber    *numfalse = [NSNumber numberWithBool:false];

    [StatusImage setImage:
        [owner batteryImage:image_index isCharging:is_charging]];

    [properties removeAllObjects];

    // AC Connected

    [properties setObject:([ACPresentCheck intValue] ? 
                    @kIOPSACPowerValue@:@kIOPSBatteryPowerValue) 
                forKey:@kIOPSPowerSourceStateKey];

    // Batt Present

    [properties setObject:([PresentCheck intValue] ? numtrue:numfalse)
                forKey:@kIOPSIsPresentKey];

    // Is Charging

    [properties setObject:([ChargingCheck intValue] ? numtrue:numfalse)
                forKey:@kIOPSIsChargingKey];

    // Capacity

    full_charge_capacity = [MaxCapCell intValue];
    [properties setObject:[NSNumber numberWithInt: full_charge_capacity ] 
                forKey:@kIOPSMaxCapacityKey];

    [properties setObject:[NSNumber numberWithInt:
                    (([PercentageSlider intValue] * full_charge_capacity)/100)]
                forKey:@kIOPSCurrentCapacityKey];

    // Time to empty
    tmp = [NSNumber numberWithInt:[TimeToEmptyCell intValue]];
    [properties setObject:tmp
                forKey:@kIOPSTimeToEmptyKey];
    
    // Time to full
    tmp = [NSNumber numberWithInt:[TimeToFullCell intValue]];
    [properties setObject:tmp
                forKey:@kIOPSTimeToFullChargeKey];
    
    // Name
    if([NameField stringValue]) {
        [properties setObject:[NameField stringValue]
                forKey:@kIOPSNameKey];
    }

    return;
}

- (void)transmitDictionaryToUPSPlugIn
{
    mach_port_t   		bootstrap_port = MACH_PORT_NULL;
    mach_port_t			connect= MACH_PORT_NULL;
    
    if( !IOUPSMIGServerIsRunning(&bootstrap_port, &connect) ) {
        NSLog(@"Disaster! No MIG Server running!\n");
    } else {
/*
        fakeups_set_properties(connect, dictionary, [dictionary getBytesCount]);
*/    
    }


}

- (void)enableAllUIInterfaces:(bool)value
{

    [ACPresentCheck setEnabled:value];
    [ChargingCheck setEnabled:value];
    [ConfidenceMenu setEnabled:value];
    [healthMenu setEnabled:value];
    [MaxCapCell setEnabled:value];
    [NameField setEnabled:value];
    [PercentageSlider setEnabled:value];
    [PresentCheck setEnabled:value];
    [StatusImage setEnabled:value];
    [TimeToEmptyCell setEnabled:value];
    [TimeToFullCell setEnabled:value];
    [TransportMenu setEnabled:value];


}

- (void)awake
{
    int image_index = [PercentageSlider intValue]/10;
    int is_charging = [ChargingCheck intValue];

    [StatusImage setImage:
        [owner batteryImage:image_index isCharging:is_charging]];

    if( !_UPSPlugInLoaded ) {
        [enabledUPSCheck setEnabled:false];
        [self enableAllUIInterfaces:false];
    } else {
        [enabledUPSCheck setEnabled:true];
        [self enableAllUIInterfaces:true];
    }

    return;
}


@end
