/* FakeBatteryObject */

#import <Cocoa/Cocoa.h>
#include "BatteryFakerWindowController.h"

@class BatteryFakerWindowController;

@interface FakeBatteryObject : NSObject
{
    IBOutlet id ACPresentCheck;
    IBOutlet id AmpsCell;
    IBOutlet id BattChargingCheck;
    IBOutlet id BattPercentageSlider;
    IBOutlet id BattPresentCheck;
    IBOutlet id BattStatusImage;
    IBOutlet id ConfidenceMenu;
    IBOutlet id CycleCountCell;
    IBOutlet id DesignCapCell;
    IBOutlet id HealthMenu;
    IBOutlet id MaxCapCell;
    IBOutlet id MaxErrCell;
    IBOutlet id VoltsCell;

    NSMutableDictionary     *properties;
    BatteryFakerWindowController    *owner;
}

- (NSDictionary *)properties;

- (void)UIchange;

- (void)awake;

@end
