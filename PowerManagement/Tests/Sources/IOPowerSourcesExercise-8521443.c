
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPowerSourcesPrivate.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitLib.h>

#include <stdio.h>
#include "PMTestLib.h"

int main()
{
    IOReturn        ret = 0;
    
     ret = PMTestInitialize("IOPowerSourcesExercise-8521443", "com.apple.iokit.powertesting");
     if(kIOReturnSuccess != ret)
     {
         fprintf(stderr,"PMTestInitialize failed with IOReturn error code 0x%08x\n", ret);
         exit(-1);
     }
    
    
    /*
    
    Call IOPSCopyPowerSourcesInfo
    PASS: It returns NON-NULL
    FAIL: It returns NULL
    
    */
    
    CFTypeRef psSnap = IOPSCopyPowerSourcesInfo();
    if (psSnap) {
        PMTestPass("IOPSCopyPowerSourcesInfo returned non-NULL\n");
    } else {
        PMTestFail("IOPSCopyPowerSourcesInfo returned NULL\n");
    }

    /*
    
    Platform model style check
    Read ACPI platform model info
    FAIL: ACPI desktop with battery support; or ACPI portable without battery support
    PASS: Otherwise
    
    */
    #define kACPIDesktopSystemType   1
    #define kACPILaptopSystemType    2
    
    io_registry_entry_t acpi_node = IO_OBJECT_NULL;
    acpi_node = IORegistryEntryFromPath(IO_OBJECT_NULL,  "IOACPIPlane:/");
    if (!acpi_node) {
        /* If !acpi_node; do nothing. This sytem doesn't have ACPI */
         PMTestLog("Skipping ACPI system type test - no \'IOACPIPlane/acpi\' found.\n");
    } else
    {
        CFNumberRef sys_id = NULL;
        sys_id = IORegistryEntryCreateCFProperty(acpi_node, CFSTR("system-type"), 0, 0);
        
        if (!sys_id) {
            PMTestFail("Found ACPI node; but could not find ACPI property system-id\n");
        } else {
            
            int *system_type = CFDataGetBytePtr(sys_id);
                        
            if ( (*system_type == kACPILaptopSystemType)
                && psSnap && !IOPSGetActiveBattery(psSnap)) 
            {
                PMTestFail("Couldn't find batteries on an ACPI Portable system. This is an inconsistency between EFI and battery drivers.\n");
            } 
            else if ( (system_type == kACPIDesktopSystemType)
                && psSnap && IOPSGetActiveBattery(psSnap))
            {
                PMTestFail("Did find batteries no an ACPI Desktop system. This is an inconsistency between EFI and battery drivers.\n");
            }else {
            
                PMTestPass("ACPI advertises system-type=%d, and batteries are correctly: %s\n",
                           *system_type, IOPSGetActiveBattery(psSnap) ? "present" : "missing");
            }
        }
    }
    
    
    /*
    
    IOPSGetProvidingPowerSourceType:
    PASS: Returns one of CFSTR(kIOPMACPowerKey), CFSTR(kIOPMBatteryPowerKey), CFSTR(kIOPMUPSPowerKey)
    FAIL: Returns anything else

     
     */
    CFStringRef providingType = NULL;
    
    providingType = IOPSGetProvidingPowerSourceType(psSnap);
    
    if (providingType && (CFEqual(providingType, CFSTR(kIOPMACPowerKey)) || CFEqual(providingType, CFSTR(kIOPMBatteryPowerKey))
        || CFEqual(providingType, CFSTR(kIOPMUPSPowerKey))))
    {
        char   myPSString[200];
        CFStringGetCString(providingType, myPSString, sizeof(myPSString), kCFStringEncodingUTF8);
        PMTestPass("IOPSGetProvidingPowerSourceType returns %s\n", myPSString);
    } else {
        PMTestFail("IOPSGetProvidingPowerSourceType returned an invalid %s return type.\n", providingType ? "non-NULL" : "NULL");
    }
    
    /* 
    
    IOPSLowBatteryWarningLevel
    PASS: Returns kIOPSLowBatteryWarningNone, kIOPSLowBatteryWarningEarly, or kIOPSLowBatteryWarningFinal
    FAIL: Returns anything else
    
    */

    IOPSLowBatteryWarningLevel wL = IOPSGetBatteryWarningLevel();
    
    if (wL == kIOPSLowBatteryWarningNone || wL == kIOPSLowBatteryWarningEarly || wL == kIOPSLowBatteryWarningFinal)
    {
        char *showString;
        
        if (wL == kIOPSLowBatteryWarningNone)
            showString = "WarningNone";
        else if (wL == kIOPSLowBatteryWarningEarly)
            showString = "WarningEarly";
        else if (wL == kIOPSLowBatteryWarningFinal)
            showString = "WarningFinal";
        
        PMTestPass("IOPSGetBatteryWarningLevel returned the valid return value %d = %s\n", (int)wL, showString);
    } else {
        
        PMTestFail("IOPSGetBatteryWarningLevel returned the non-meaningful value %d\n", (int)wL);
    }

    /* Sanity cross check: Does BatteryWarningLevel match GetProvidingPowerSourceType?
     */
     
    if ( CFEqual(providingType, CFSTR(kIOPMACPowerKey))
        && (wL != kIOPSLowBatteryWarningNone))
    {
        PMTestFail("API Inconsistency: Providing power type AC Power doesn't match warningLevel = %d", wL);
    }
     
    
    /*
     
     IOPSGetTimeRemainingEstimate
     PASS: Returns kIOPSTimeRemainingUnknown, kIOPSTimeRemainingUnlimited, 
        or a non-negative integer.
     FAIL: Returns a negative number < -2.0
     FAIL: Returns a positive number > 1 year = 365 * 24 * 60 * 60
    
     */
     
    CFTimeInterval t = 0.0;
    
    t = IOPSGetTimeRemainingEstimate();
    
    if (t < -2.0) {
        PMTestFail("IOPSGetTimeRemaining returns a negative number less than -2.0. Returns=%f\n", t);
    } else if (t > (365.0*24.0*60.0*60.0)) {
        PMTestFail("IOPSGetTimeRemaining fails a sanity check and returns a number > 1 year. Returns=%d\n", t);
    } else if (t == kIOPSTimeRemainingUnlimited || t == kIOPSTimeRemainingUnknown || t >= 0.0) 
    {
        char *showString;
        char buf[99];
        bool showMinutes = false;
        
        if (t == kIOPSTimeRemainingUnlimited) {
            showString = "Unlimited";
        } else if (t == kIOPSTimeRemainingUnknown) {
            showString = "Unknown";
        } else { 
            snprintf(buf, sizeof(buf), "hh:mm = %02d:%02d", (int)(t / 3600), (int)(t / 3600)%60);
            showString = buf;
        }
                
            PMTestPass("IOPSGetTimeRemaining returns valid return code %f: %s\n", t, showString);
    } else {
        PMTestFail("IOPSGetTimeRemaining unexpected return code %f.\n", t);
    }

    /*
     * Sanity check: ProvidingPowerSourceType vs TimeRemainingEstimate
     */
    if ( CFEqual(providingType, CFSTR(kIOPMACPowerKey))
        && (t != kIOPSTimeRemainingUnlimited))
    {
        PMTestFail("API Inconsistency: Providing power type ACPower doesn't match TimeRemainingEstimate = %4.2f (should be -2.0=Unlimited)\n", t);
    } else if (!CFEqual(providingType, CFSTR(kIOPMACPowerKey))
        && (t == kIOPSTimeRemainingUnlimited))
    {   
        PMTestFail("API Inconsistency: Providing power type BatteryPower(?) doesn't match TimeRemainingEstimate = %4.2f (should be -1.0=Unknown, or a time)\n", t);
    }
               
    
    /*
    
    IOPSCopyExternalPowerAdapterDetails
    PASS: Always pass. Either NULL or a valid CFDictionary, is acceptable return value.
    Just call it to exercise the machinery.
     
     */

    IOPSCopyExternalPowerAdapterDetails();
        
    PMTestPass("IOPSCopyExternalPowerAdapterDetails() ran, and the world didn't blow up.\n");
        
    return 0;
}