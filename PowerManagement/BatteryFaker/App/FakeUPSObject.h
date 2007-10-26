/* FakeUPSObject */

#import <Cocoa/Cocoa.h>

#import "BatteryFakerWindowController.h"
#import <IOKit/IOCFPlugin.h>
#import "../IOUPSPlugIn/IOUPSPlugin.h"

@class BatteryFakerWindowController;

@interface FakeUPSObject : NSObject
{
    IBOutlet id ACPresentCheck;
    IBOutlet id ChargingCheck;
    IBOutlet id ConfidenceMenu;
    IBOutlet id enabledUPSCheck;
    IBOutlet id healthMenu;
    IBOutlet id MaxCapCell;
    IBOutlet id NameField;
    IBOutlet id PercentageSlider;
    IBOutlet id PresentCheck;
    IBOutlet id StatusImage;
    IBOutlet id TimeToEmptyCell;
    IBOutlet id TimeToFullCell;
    IBOutlet id TransportMenu;

    NSMutableDictionary *properties;
    BatteryFakerWindowController *owner;
    
    bool            _UPSPlugInLoaded;
}

- (NSDictionary *)properties;

- (void)UIchange;

- (void)awake;

- (void)enableAllUIInterfaces:(bool)value;

@end
