#import "BatteryFakerWindowController.h"

#import <ApplicationServices/ApplicationServicesPriv.h>
#import <IOKit/pwr_mgt/IOPM.h>
#import <IOKit/IOKitLib.h>
#import <unistd.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/ps/IOPowerSources.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>


#define kLoadArg                    "load"
#define kUnloadArg                  "unload"
#define kKickBattMonArg             "kickbattmon"

#define kMBBBundleID				CFSTR("com.apple.menuextra.battery")
#define kMBBBundlePath				CFSTR("/System/Library/CoreServices/Menu Extras/Battery.menu")

static const char *kextBundleID = "com.apple.driver.BatteryFaker";
static BatteryFakerWindowController *gBFWindowController = NULL;

static kmod_info_t * kmod_get_loaded(unsigned int * kmod_count);
static int kmod_ref_compare(const void * a, const void * b);
static void usage(int level);

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
- awakeFromNib
{
    // Send battery the defaults
    [batt setPropertiesAndUpdate:
        [self batterySettingsFromWindow]];

//  UPS remains not-implemented
//    [ups awake];
    
    [self runSUIDTool:kLoadArg];

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

    return nil;
}

/******************************************************************************
 *
 * window close handler
 *
 ******************************************************************************/
- (void)windowWillClose:(NSNotification *)notification
{
    /* attempt to unload BatteryFaker.kext */
    [self runSUIDTool:kUnloadArg];
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
    [self runSUIDTool:kKickBattMonArg];
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

    NSLog(@"Minutes remaining = %d\n", minutes_remaining);

    // Tell battery kext object
    [batt setPropertiesAndUpdate:uiProperties];
    
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
    
    NSNumber    *numtrue = [NSNumber numberWithBool:true];
    NSNumber    *numfalse = [NSNumber numberWithBool:false];
    
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
    
    if( is_kext_loaded( kextBundleID ) ) 
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

    char        suidToolPath[255];
    char        kextPath[255];
    
    /* suidToolPath is in:
     * BatteryFaker.app/Contents/Resources/suidToolPath
     */
    [[[NSBundle mainBundle] resourcePath] 
                getCString:suidToolPath maxLength:255 
                encoding:NSUTF8StringEncoding];
    strcat(suidToolPath, "/suidLauncherTool");

    /* BatterFaker.kext
     */
    [[[NSBundle mainBundle] resourcePath] 
                getCString:kextPath maxLength:255 
                encoding:NSUTF8StringEncoding];
    strcat(kextPath, "/BatteryFaker.kext");
            
    pid = fork();
  
    if (pid == 0)
    {
        /*   Run as root via 'suidToolPath'
         *   fakerloader.sh [load/unload] /path/to/BatteryFaker.app
         */
         
        if(0 == strcmp( kKickBattMonArg, loadArg ))
        {
            result = execl( suidToolPath, suidToolPath, 
                            kKickBattMonArg, NULL);
            
            /* 
             * debug
             */
            printf("%s %s\n", suidToolPath, kKickBattMonArg);
        } else {
            /* loadArg == ('load' or 'unload') */
            result = execl( suidToolPath, suidToolPath, 
                            loadArg, kextPath, NULL);
            /* 
             * debug
             */
            printf("%s %s %s\n", suidToolPath, loadArg, kextPath);
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
int is_kext_loaded(const char * bundle_id)
{
    int result = 0;
    unsigned int num_kexts = 0;
    kmod_info_t * kexts = NULL;  // must free
    unsigned int i;

    kexts = kmod_get_loaded(&num_kexts);
    if (!kexts) {
        goto finish;
    }

    for (i = 0; i < num_kexts; i++) {
        if (strcmp(bundle_id, kexts[i].name) == 0) {
            result = 1;
            goto finish;
        }
    }
finish:
    if (kexts) free(kexts);
    return result;
}

/*******************************************************************************
* kmod_get_loaded()
*
* This function retrieves the list of loaded kmods from the kernel and massages
* it into a more user-friendly form, including copying the kernel data from
* a vm_allocated buffer into a regular malloc one.
*******************************************************************************/
static kmod_info_t * kmod_get_loaded(unsigned int * num_kmods)
{
    kmod_info_t * kmod_list_returned = NULL;  // returned

    mach_port_t host_port = MACH_PORT_NULL;
    kern_return_t mach_result = KERN_SUCCESS;
    kmod_info_t * kmod_list = NULL;  // must vm_deallocate()
    unsigned int kmod_bytecount;  // not really used
    unsigned int kmod_count;
    kmod_info_t * this_kmod;
    kmod_reference_t * kmod_ref;
    int ref_count;
    int i, j;

   /* Get the list of loaded kmods from the kernel.
    */
    host_port = mach_host_self();
    mach_result = kmod_get_info(host_port, (void *)&kmod_list,
        &kmod_bytecount);
    if (mach_result != KERN_SUCCESS) {
        NSLog(
            @"couldn't get list of loaded kexts from kernel - %s\n",
            mach_error_string(mach_result));
        goto finish;
    }

    kmod_list_returned = (kmod_info_t *)malloc(kmod_bytecount);
    if (!kmod_list_returned) {
        NSLog(@"memory allocation failure\n");
        goto finish;
    }

    memcpy(kmod_list_returned, kmod_list, kmod_bytecount);

   /* kmod_get_info() doesn't return a proper count so we have
    * to scan the array checking for a NULL next pointer.
    */
    this_kmod = kmod_list_returned;
    kmod_count = 0;
    while (this_kmod) {
        kmod_count++;
        this_kmod = (this_kmod->next) ? (this_kmod + 1) : 0;
    }

    if (num_kmods) {
        *num_kmods = kmod_count;
    }

   /* rebuild the reference lists from their serialized pileup
    * after the list of kmod_info_t structs.
    */
    this_kmod = kmod_list_returned;
    kmod_ref = (kmod_reference_t *)(kmod_list_returned + kmod_count);
    while (this_kmod) {

       /* How many refs does this kmod have? Again, kmod_get_info ovverrides
        * a field. Here what is the actual reference list in the kernel becomes
        * the count of references tacked onto the end of the kmod_info_t list.
        */
        ref_count = (int)this_kmod->reference_list;
        if (ref_count) {
            this_kmod->reference_list = kmod_ref;

            for (i = 0; i < ref_count; i++) {
                int foundit = 0;
                for (j = 0; j < kmod_count; j++) {
                   /* kmod_get_info() made each kmod_info_t struct's .next field
                    * point to itself IN KERNEL SPACE, so this is a sort of id
                    * for the reference list. Here we replace the ref's
                    * info field, a here-useless KERNEL SPACE ADDRESS,
                    * with the list id of the kmod_info_t struct.
                    * Gross, gross hack.
                    */
                    if (kmod_ref->info == kmod_list_returned[j].next) {
                        kmod_ref->info = (kmod_info_t *)kmod_list_returned[j].id;
                        foundit++;
                        break;
                    }
                }

               /* If we didn't find it, that's because the last entry's next
                * pointer is SET TO ZERO to signal the end of the kmod_info_t
                * list, even though the same field is used for other purposes
                * in every other entry in the list. So set the ref's info
                * field to the id of the last entry in the list.
                */
                if (!foundit) {
                    kmod_ref->info =
                        (kmod_info_t *)kmod_list_returned[kmod_count - 1].id;
                }

                kmod_ref++;
            }

           /* Sort the references in descending order of reference index.
            */
            qsort(this_kmod->reference_list, ref_count,
                  sizeof(kmod_reference_t), kmod_ref_compare);

           /* Patch up the links between ref structs and move on to the
            * next one.
            */
            for (i = 0; i < ref_count - 1; i++) {
                this_kmod->reference_list[i].next =
                    &this_kmod->reference_list[i+1];
            }
            this_kmod->reference_list[ref_count - 1].next = 0;
        }
        this_kmod  = (this_kmod->next) ? (this_kmod + 1) : 0;
    }

finish:
   /* Dispose of the host port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (MACH_PORT_NULL != host_port) {
        mach_port_deallocate(mach_task_self(), host_port);
    }

    if (kmod_list != NULL) {
        vm_deallocate(mach_task_self(), (vm_address_t)kmod_list,
            kmod_bytecount);
    }

    return kmod_list_returned;
}

/*******************************************************************************
* Used for sorting kmod reference lists.
*******************************************************************************/
static int kmod_ref_compare(const void * a, const void * b)
{
    kmod_reference_t * r1 = (kmod_reference_t *)a;
    kmod_reference_t * r2 = (kmod_reference_t *)b;
    // these are load indices, not CFBundleIdentifiers
    // sorting high-low.
    return ((int)r2->info - (int)r1->info);
}

