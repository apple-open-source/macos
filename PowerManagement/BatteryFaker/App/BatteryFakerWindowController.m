/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#import <ApplicationServices/ApplicationServicesPriv.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/pwr_mgt/IOPMLibPrivate.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/kext/OSKext.h>
#import <System/libkern/OSReturn.h>

#import <unistd.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/mach_host.h>

#import "BatteryFakerWindowController.h"

#define kLoadArg                    "load"
#define kUnloadArg                  "unload"
#define kKickBattMonArg             "kickbattmon"

#define kMBBBundleID				CFSTR("com.apple.menuextra.battery")
#define kMBBBundlePath				CFSTR("/System/Library/CoreServices/Menu Extras/Battery.menu")

#define kKextBundleID               CFSTR("com.apple.driver.BatteryFaker")
static BatteryFakerWindowController *gBFWindowController = NULL;

static int is_kext_loaded(CFStringRef bundle_id);

void powerSourceChangeCallBack(void *in);


@implementation BatteryFakerWindowController

/******************************************************************************
 *
 * App init
 *
 ******************************************************************************/
- (id)init
{
    NSBundle    *myBundle = [NSBundle mainBundle];
    NSString    *path;
    
    
    path = [myBundle pathForResource:@"Single_0" ofType:@"tif"];
    batteryImage[0] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_1" ofType:@"tif"];
    batteryImage[1] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_2" ofType:@"tif"];
    batteryImage[2] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_3" ofType:@"tif"];
    batteryImage[3] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_4" ofType:@"tif"];
    batteryImage[4] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_5" ofType:@"tif"];
    batteryImage[5] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_6" ofType:@"tif"];
    batteryImage[6] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_7" ofType:@"tif"];
    batteryImage[7] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_8" ofType:@"tif"];
    batteryImage[8] = [[NSImage alloc] initWithContentsOfFile:path];


    path = [myBundle pathForResource:@"Single_Charge_0" ofType:@"tif"];
    chargingImage[0] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_1" ofType:@"tif"];
    chargingImage[1] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_2" ofType:@"tif"];
    chargingImage[2] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_3" ofType:@"tif"];
    chargingImage[3] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_4" ofType:@"tif"];
    chargingImage[4] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_5" ofType:@"tif"];
    chargingImage[5] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_6" ofType:@"tif"];
    chargingImage[6] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_7" ofType:@"tif"];
    chargingImage[7] = [[NSImage alloc] initWithContentsOfFile:path];
    path = [myBundle pathForResource:@"Single_Charge_8" ofType:@"tif"];
    chargingImage[8] = [[NSImage alloc] initWithContentsOfFile:path];
    
    gBFWindowController = [super init];
    
    CFRunLoopSourceRef      rls;
    rls = IOPSNotificationCreateRunLoopSource(powerSourceChangeCallBack, NULL);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    
    
    return gBFWindowController;
}

/******************************************************************************
 *
 * App awakefromNib
 *
 ******************************************************************************/
- (void)awakeFromNib
{
    // Send battery the defaults
    [batt setPropertiesAndUpdate: [self batterySettingsFromWindow]];


    /* Trigger BatteryFaker.kext load with help of IOPMrootDomain
     */
    io_registry_entry_t root_domain = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IOPMrootDomain"));    
    if (MACH_PORT_NULL != root_domain)
    {
        kern_return_t       ret;
        ret = IORegistryEntrySetCFProperty(root_domain, CFSTR("SoftwareSimulatedBatteries"), kCFBooleanTrue);
        if (KERN_SUCCESS != ret) {
            NSLog(@"BatteryFaker error initializing \"SoftwareSimulatedBatteries\" resource 0x%x\n", ret);
        }
        IOObjectRelease(root_domain);
    }
    
    /* Signal to PM Configd that we're running, and we want debug power sources instead of real power sources.
     */
    IOReturn ret;
    IOPMAssertionID id = kIOPMNullAssertionID;

    ret = IOPMAssertionCreateWithName(
                kIOPMAssertionTypeDisableRealPowerSources_Debug,
                kIOPMAssertionLevelOn,
                CFSTR("com.apple.iokit.powermanagement.assert_tool"),
                &id);

    if (kIOReturnSuccess != ret || kIOPMNullAssertionID == id)
    {
        NSLog(@"IOPMAssertionCreate failed: asserting %@ returned 0x%08x\n", kIOPMAssertionTypeDisableRealPowerSources_Debug, ret);
    }
    
    [self updateKEXTLoadStatus];

    [self updateUPSPlugInStatus];
    
    snapshotController = [[BatterySnapshotController alloc] init];

    [self populateSnapshotMenuBar];

    /* Adjust battery image */
    int image_index = [BattPercentageSlider intValue]/10;
    int is_charging = [BattChargingCheck intValue];
    
    [BattStatusImage setImage:
        [self batteryImage:image_index isCharging:is_charging]];

    [self setHealthConditionString:nil];

    return;
}

/******************************************************************************
 *
 * window close handler
 *
 ******************************************************************************/
- (void)windowWillClose:(NSNotification *)notification
{
    /* attempt to unload BatteryFaker.kext */

    // TODO: Unload battery monitor
}


/******************************************************************************
 *
 * UIchange - read UI state from window, blast into BatteryFaker.kext
 * and/or UPSFaker.plugin
 *
 ******************************************************************************/
- (IBAction)UIchange:(id)sender
{
    NSDictionary    *batterySettings = nil;

    // Let each battery re-sync with its UI
    batterySettings = [self batterySettingsFromWindow];
    [batt  setPropertiesAndUpdate:batterySettings];
    
    [activeSnapshotTextID setStringValue:@"Active Snapshot: None Selected"];

    [ups UIchange];
    
    [self updateKEXTLoadStatus];

    return;
}


/******************************************************************************
 *
 * Menu Item Actions
 *
 ******************************************************************************/

- (IBAction)kickBatteryMonitorMenu:(id)sender
{
    // TODO: kick battery monitor
//    [self runSUIDTool:kKickBattMonArg];
}

- (IBAction)enableMenuExtra:(id)sender
{
    CFURLRef thePath = CFURLCreateWithFileSystemPath(
                            kCFAllocatorDefault, kMBBBundlePath, 
                            kCFURLPOSIXPathStyle, NO);
    if (nil != thePath)
    {
        CoreMenuExtraAddMenuExtra( thePath, 
                            kCoreMenuExtraBatteryPosition, 
                            0, nil, 0, nil);
        CFRelease(thePath);
    }
}

- (IBAction)disableMenuExtra:(id)sender
{
    UInt32 theResult = 0;
	if ( !CoreMenuExtraGetMenuExtra(kMBBBundleID, &theResult) )
	{
        CoreMenuExtraRemoveMenuExtra(theResult, nil);
	}
}

- (IBAction)openEnergySaverMenu:(id)sender
{
    NSLog(@"Opening Energy Saver Menu!\n");
}

- (IBAction)snapshotSelectedMenuAction:(id)sender
{
    NSDictionary    *snapshotDescription = 
                    [snapshotController batterySnapshotForTitle:[sender title]];


    [batt setPropertiesAndUpdate:snapshotDescription];
    [self updateBatteryUI:snapshotDescription];
}

/******************************************************************************
 *
 * batterySettingsFromWindow
 * pulls battery settings in from the UI and puts them in a dictionary
 * Using IOPM.h kIOPMPS keys to label each setting
 *
 ******************************************************************************/

- (NSDictionary *)batterySettingsFromWindow
{
    NSMutableDictionary *uiProperties = nil;

    int image_index = [BattPercentageSlider intValue]/10;
    int is_charging = [BattChargingCheck intValue];
    int full_charge_capacity = 0;
    int current_capacity = 0;
    int mAh_remaining = 0;
    int amperage = 0;
    int minutes_remaining = 0;
    
    NSNumber    *numtrue = [NSNumber numberWithBool:true];
    NSNumber    *numfalse = [NSNumber numberWithBool:false];
    
    // Update battery image UI 
    [BattStatusImage setImage:
        [self batteryImage:image_index isCharging:is_charging]];

    uiProperties = [NSMutableDictionary dictionary];

    // AC Connected
    [uiProperties setObject:([ACPresentCheck intValue] ? numtrue : numfalse) 
                forKey:@kIOPMPSExternalConnectedKey];
    // Batt Present
    [uiProperties setObject:([BattPresentCheck intValue] ? numtrue : numfalse)
                forKey:@kIOPMPSBatteryInstalledKey];
    // Is Charging
    [uiProperties setObject:([BattChargingCheck intValue] ? numtrue : numfalse)
                forKey:@kIOPMPSIsChargingKey];
    // Design Capacity
    [uiProperties setObject:[NSNumber numberWithInt:[DesignCapCell intValue]] 
                forKey:@"DesignCapacity"];
    // Max Capacity
    full_charge_capacity = [MaxCapCell intValue];
    [uiProperties setObject:[NSNumber numberWithInt: full_charge_capacity ] 
                forKey:@kIOPMPSMaxCapacityKey];
    // Current Capacity
    current_capacity = ([BattPercentageSlider intValue] * full_charge_capacity)/100;
    [uiProperties setObject:[NSNumber numberWithInt:current_capacity]
                forKey:@kIOPMPSCurrentCapacityKey];
    // Amperage
    amperage = [AmpsCell intValue];
    [uiProperties setObject:[NSNumber numberWithInt:amperage] 
                forKey:@kIOPMPSAmperageKey];
    // Voltage
    [uiProperties setObject:[NSNumber numberWithInt:[VoltsCell intValue]] 
                forKey:@kIOPMPSVoltageKey];
    // Cycle Count
    [uiProperties setObject:[NSNumber numberWithInt:[CycleCountCell intValue]] 
                forKey:@kIOPMPSCycleCountKey];
    // Max Err    
    [uiProperties setObject:[NSNumber numberWithInt:[MaxErrCell intValue]] 
                forKey:@"MaxErr"];
                
    // PFStatus    
    [uiProperties setObject:[NSNumber numberWithInt:[PFStatusCell intValue]] 
                forKey:@"PermanentFailureStatus"];
    // Error Condition
    [uiProperties setObject:[ErrorConditionCell stringValue] forKey:@kIOPMPSErrorConditionKey];
    // DesignCapacity 0x70    
    [uiProperties setObject:[NSNumber numberWithInt:[DesignCap0x70Cell intValue]] 
                forKey:@"DesignCycleCount70"];
    // DesignCapacity 0x9C
    [uiProperties setObject:[NSNumber numberWithInt:[DesignCap0x9CCell intValue]] 
                forKey:@"DesignCycleCount9C"];


    // Time Remaining
    if (is_charging) {
        mAh_remaining = full_charge_capacity - current_capacity;
    } else {
        mAh_remaining = -1*current_capacity;
    }
    if (mAh_remaining == 0 || amperage == 0) {
        minutes_remaining = 0;
    } else {
        minutes_remaining = mAh_remaining / amperage;
    }
    [uiProperties setObject:[NSNumber numberWithInt:minutes_remaining] 
            forKey:@kIOPMPSTimeRemainingKey];

//    NSLog(@"Minutes remaining = %d\n", minutes_remaining);

    // Tell battery kext object
    [batt setPropertiesAndUpdate:uiProperties];
    
    return uiProperties;
}

/******************************************************************************
 *
 * updateBatteryUI
 * make UI reflect this dictionary of settings
 *
 ******************************************************************************/

- (void)updateBatteryUI:(NSDictionary *)newUI
{
    int image_index = [BattPercentageSlider intValue]/10;
    int is_charging = [BattChargingCheck intValue];
    int full_charge_capacity = 0;
    int design_capacity = 0;
    int32_t intValue = 0;
        
    // Update "Active Snapshot" title:
    [activeSnapshotTextID setStringValue:
        [@"Active Snapshot: " stringByAppendingString:[newUI objectForKey:@"SnapshotTitle"]]];
    [activeSnapshotTextID setHidden:FALSE];
    [activeSnapshotTextID setEnabled:TRUE];
    
    // Update battery image UI 
    [BattStatusImage setImage:
        [self batteryImage:image_index isCharging:is_charging]];

    // AC Connected
    intValue =  [[newUI objectForKey:@kIOPMPSExternalConnectedKey] intValue];
    [ACPresentCheck setIntValue:intValue];
    
    // Batt Present
    intValue = [[newUI objectForKey:@kIOPMPSBatteryInstalledKey] intValue];
    [BattPresentCheck setIntValue:intValue];

    // Is Charging
    intValue = [[newUI objectForKey:@kIOPMPSIsChargingKey] intValue];
    [BattChargingCheck setIntValue:intValue];

    // Design Capacity
    design_capacity = [[newUI objectForKey:@"DesignCapacity"] intValue];
    [DesignCapCell setIntValue:design_capacity];

    // Max Capacity
    full_charge_capacity = [[newUI objectForKey:@kIOPMPSMaxCapacityKey] intValue];
    [MaxCapCell setIntValue:full_charge_capacity];


    // Update "FCC/DC = X%" text
    [FCCOverDCTextID setStringValue:
        [@"Capacity Ratio = " stringByAppendingFormat:@"%d%%", (100 * (full_charge_capacity + 200))/design_capacity]];

    // Current Capacity
    intValue = [[newUI objectForKey:@kIOPMPSCurrentCapacityKey] intValue];
    if (0 != full_charge_capacity) {
        intValue = (intValue *100) / full_charge_capacity;
    } else {
        intValue = 0;
    }
    [BattPercentageSlider setIntValue:intValue];

    // Amperage
    intValue = [[newUI objectForKey:@kIOPMPSAmperageKey] intValue];
    [AmpsCell setIntValue:intValue];

    // Voltage
    intValue = [[newUI objectForKey:@kIOPMPSVoltageKey] intValue];
    [VoltsCell setIntValue:intValue];

    // Cycle Count
    intValue = [[newUI objectForKey:@kIOPMPSCycleCountKey] intValue];
    [CycleCountCell setIntValue:intValue];

    // Max Err    
    intValue = [[newUI objectForKey:@"MaxErr"] intValue];
    [MaxErrCell setIntValue:intValue];

}

/******************************************************************************
 *
 * batteryImage
 * accessor for appropriate image for battery %
 *
 ******************************************************************************/

// Accessor
- (NSImage *)batteryImage:(int)i isCharging:(bool)c
{
    if(i<0 || i>10) return nil;
    if(c) {
        if(i >= 8)
            return chargingImage[8];
        else
            return chargingImage[i];
    } else {
        if(i >= 8)
            return batteryImage[8];
        else
            return batteryImage[i];
    }
}

/******************************************************************************
 *
 * populateSnapshotMenuBar
 *
 ******************************************************************************/
- (void)populateSnapshotMenuBar
{
    NSArray *menuItems = [snapshotController menuTitlesForSnapshots];
    NSMenu  *snapshotsMenu = [[NSMenu alloc] initWithTitle:@"Snapshots"];
    NSString    *addTitle = nil;
    int i;
    
    for(i = 0; i < [menuItems count]; i++)
    {            
            addTitle = [menuItems objectAtIndex:i];
     
            if ([addTitle isEqualTo:@"Menu Separator"]) {
            
                [snapshotsMenu addItem:[NSMenuItem separatorItem]];
            
            } else {                              
                if (![snapshotsMenu addItemWithTitle:addTitle
                                    action:@selector(snapshotSelectedMenuAction:)
                                    keyEquivalent:@""])
                {
                    NSLog(@"error %@\n", [menuItems objectAtIndex:i]);
                }
            }
    }
    
    [snapshotMenuID setSubmenu:snapshotsMenu];
    [snapshotsMenu release];
    
    return;
}

/******************************************************************************
 *
 * updateKEXTLoadStatus
 *
 ******************************************************************************/
- (void)updateKEXTLoadStatus
{
    // Time to publish whether the kext is loaded
    
    if( is_kext_loaded( kKextBundleID ) ) 
    {
        [BattFakerKEXTStatus setStringValue:@"Yes"];
    } else {
        // Make it say "No" in red!
        NSMutableAttributedString *noString = 
            [[NSMutableAttributedString alloc] initWithString:@"No"];
        [noString addAttribute: NSForegroundColorAttributeName
                        value: [NSColor redColor]
                        range: NSMakeRange(0, [noString length])];

        [BattFakerKEXTStatus setStringValue: (NSString *)noString];

        [noString release];
    }

    return;
}

/******************************************************************************
 *
 * setHealthString
 *
 ******************************************************************************/
- (void)setHealthString:(NSString *)healthString
{
    if (!healthString)
        return;
        
    [HealthTextID setStringValue:[@"Health = " stringByAppendingString:healthString]];
}

/******************************************************************************
 *
 * setHealthConditionString
 *
 ******************************************************************************/
- (void)setHealthConditionString:(NSString *)healthConditionString
{
    if (!healthConditionString) {
        [ConditionTextID setEnabled:FALSE];
        return;
    }    
    
    [ConditionTextID setEnabled:TRUE];
    
    [ConditionTextID setStringValue:[@"Condition = " stringByAppendingString:healthConditionString]];
}



/******************************************************************************
 *
 * powerSourceChangeCallBack
 *
 ******************************************************************************/
void powerSourceChangeCallBack(void *in __unused)
{
    CFTypeRef           blob = NULL;
    NSArray             *list = NULL;
    NSDictionary        *one_ps = NULL;
    NSString            *healthString = NULL;
    NSString            *conditionString = NULL;

    if (!gBFWindowController)
        return;

    blob = IOPSCopyPowerSourcesInfo();
    list = (NSArray *)IOPSCopyPowerSourcesList(blob);
    if (!blob || !list)
        return;
        
    one_ps = (NSDictionary *)IOPSGetPowerSourceDescription(blob, [list objectAtIndex: 0]);
    healthString = [one_ps objectForKey:@"BatteryHealth"];
    [gBFWindowController setHealthString:healthString];

    conditionString = [one_ps objectForKey:@"BatteryHealthCondition"];
    [gBFWindowController setHealthConditionString:conditionString];
    
    CFRelease(blob);
    [list release];
}

/******************************************************************************
 *
 * updateUPSPlugInStatus
 *
 ******************************************************************************/
- (void)updateUPSPlugInStatus
{
    bool upsPlugInLoaded = false;

    if( upsPlugInLoaded) {
        [UPSPlugInStatus setStringValue:@"Yes"];
    } else {
        // Make it say "No" in red!
        NSMutableAttributedString *noString = 
            [[NSMutableAttributedString alloc] initWithString:@"No"];
        [noString addAttribute: NSForegroundColorAttributeName
                        value: [NSColor redColor]
                        range: NSMakeRange(0, [noString length])];

        [UPSPlugInStatus setStringValue: (NSString *)noString];

        [noString release];
    }
}

/******************************************************************************
 *
 * runSUIDTool
 *
 * exec's suidLauncherTool with an argument of 'load', 'unload',
 *  or 'kickbattmon'
 *
 ******************************************************************************/
- (int)runSUIDTool:(char *)loadArg
{    
    int         pid;
    int         result = -1;
    union       wait status;

    char        suidToolPathCSTR[255];
    char        kextPathCSTR[255];
    
    NSString    *bundleResourcesPath = NULL;
    NSString    *suidLauncherPath = NULL;
    NSString    *batteryKextPath = NULL;
    
    bundleResourcesPath = [[NSBundle mainBundle] resourcePath];

    /* suidToolPath is in:
     * BatteryFaker.app/Contents/Resources/suidToolPath
     */
    suidLauncherPath = [bundleResourcesPath
                            stringByAppendingPathComponent:@"suidLauncherTool"];
    [suidLauncherPath getCString:suidToolPathCSTR maxLength:255 
                            encoding:NSUTF8StringEncoding];

    /* BatterFaker.kext
     */
    batteryKextPath = [bundleResourcesPath
                            stringByAppendingPathComponent:@"BatteryFaker.kext"];
    [batteryKextPath getCString:kextPathCSTR maxLength:255
                            encoding:NSUTF8StringEncoding];
            
    pid = fork();
  
    if (pid == 0)
    {
        /*   Run as root via 'suidToolPath'
         *   fakerloader.sh [load/unload] /path/to/BatteryFaker.app
         */
         
        if(0 == strcmp( kKickBattMonArg, loadArg ))
        {
            result = execl( suidToolPathCSTR, suidToolPathCSTR, 
                            kKickBattMonArg, NULL);
            
            /* 
             * debug
             */
            printf("%s %s\n", suidToolPathCSTR, kKickBattMonArg);
        } else {
            /* loadArg == ('load' or 'unload') */
            result = execl( suidToolPathCSTR, suidToolPathCSTR, 
                            loadArg, kextPathCSTR, NULL);
            /* 
             * debug
             */
            printf("%s %s %s\n", suidToolPathCSTR, loadArg, kextPathCSTR);
        }
        
        if (-1 == result) 
        {
            printf("Error %d from execvp \"%s\"\n", result, strerror(result));
            printf("errno = %d\n", errno);
            exit(errno);
        }
        
        /* We can only get here if the exec failed */
        goto exit;
    }

    if (pid == -1)
    {
        result = errno;
        goto exit;
    }

    /* Success! */
    if ((wait4(pid, (int *) & status, 0, NULL) == pid) && (WIFEXITED(status)))
    {
        result = status.w_retcode;
    }
    else
    {
        result = -1;
    }
exit:
    return result;
}


@end

/*******************************************************************************
* is_kext_loaded() returns nonzero if the given bundle ID is among those
* loaded in the kernel, and zero if not (or on failure). You might want to
* change this to return -1 if an error occurs.
*******************************************************************************/
int is_kext_loaded(CFStringRef bundle_id)
{
    NSArray *kextIdentifiers = [NSArray arrayWithObject:(NSString *)bundle_id];
    NSArray *loadedKextInfo = (NSArray *)OSKextCreateLoadedKextInfo((CFArrayRef)kextIdentifiers);
    bool myKextIsLoaded = false;

    if (loadedKextInfo)
    {
        myKextIsLoaded = ([loadedKextInfo count] > 0);
        [loadedKextInfo release];
    }
    
    return myKextIsLoaded;
}


