/* FakeBatteryObject */

#import <Cocoa/Cocoa.h>
#include "BatteryFakerWindowController.h"

@class BatteryFakerWindowController;

@interface FakeBatteryObject : NSObject
{
    NSMutableDictionary     *properties;
    BatteryFakerWindowController    *owner;
}

- (NSDictionary *)properties;

- (void)setPropertiesAndUpdate:(NSDictionary *)newProperties;

@end
