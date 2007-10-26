/* BatteryFakerWindowController */

#import <Cocoa/Cocoa.h>
#import "FakeBatteryObject.h"
#import "FakeUPSObject.h"

@class FakeBatteryObject;
@class FakeUPSObject;

@interface BatteryFakerWindowController : NSWindowController
{
    IBOutlet id BattFakerKEXTStatus;
    IBOutlet id UPSPlugInStatus;
    
    FakeBatteryObject        *batt;
    FakeUPSObject            *ups;
    
    NSImage     *batteryImage[10];
    NSImage     *chargingImage[10];
}

- (NSImage *)batteryImage:(int)i isCharging:(bool)c;

- (IBAction)UIchange:(id)sender;

- (IBAction)kickBatteryMonitorMenu:(id)sender;

- (IBAction)enableMenuExtra:(id)sender;

- (IBAction)disableMenuExtra:(id)sender;

- (IBAction)openEnergySaverMenu:(id)sender;

- (IBAction)scenarioSelector:(id)sender;

- (int)runSUIDTool:(char *)loadArg;

- (void)updateKEXTLoadStatus;

- (void)updateUPSPlugInStatus;

- (void)windowWillClose:(NSNotification *)notification;

@end
