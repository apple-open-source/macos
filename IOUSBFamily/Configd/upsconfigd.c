/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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


// To Do:  Add the following configuration cookies
//
/*	Configuration info............
                Not currently used...............
    IOHIDElementCookie		rechargableCookie;
    IOHIDElementCookie		remainingCapacityLimitCookie;
    IOHIDElementCookie		warningCapacityLimitCookie;

            case kHIDUsage_BS_WarningCapacityLimit:
                upsDataRef->warningCapacityLimitCookie = cookie;
                break;
            case kHIDUsage_BS_RemainingCapacityLimit:
                upsDataRef->remainingCapacityLimitCookie = cookie;
                break;
            case kHIDUsage_BS_Rechargable:
                upsDataRef->rechargableCookie = cookie;
                break;
			g.devices[index].powerSourcePB.lowWarnLevel = 50;				// Default Value
			g.devices[index].powerSourcePB.deadWarnLevel = 20;				// Default Value
OSStatus	USBPowerGetCapacityLimits (USBReference inReference, UInt32 *warningLevel, UInt32 *shutdownLevel)
			*warningLevel = g.devices[index].powerSourcePB.lowWarnLevel;
			*shutdownLevel = g.devices[index].powerSourcePB.deadWarnLevel;
*/


// Definition of STAND_ALONE_TEST_TOOL is in command line additions for both configd targets.
// Note that to run this a standalone tool, you need to run it as root, as the SC calls are privileged.
//
#ifndef STAND_ALONE_TEST_TOOL
    #define STAND_ALONE_TEST_TOOL 0
#endif

#if STAND_ALONE_TEST_TOOL
    // Always 0 for test tool
    //
    #define UPS_DEBUG 0
	
    // 1 = Generate a debug version of the ups notification tool
    //
    #define UPS_TOOL_DEBUG 1
#else
    // 1 = Generate a debug version of the plugin
    //
    #define UPS_DEBUG 0
	
    // Always 0 for plugin
    //
    #define UPS_TOOL_DEBUG 0
#endif

//================================================================================================
//   Includes
//================================================================================================
//
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#if UPS_DEBUG
    #include <SystemConfiguration/SCPrivate.h>
#endif

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/usb/IOUSBLib.h>

//================================================================================================
//   Typedefs and Defines
//================================================================================================
//
#define	HID_MANAGER_ELEMENTS_INITIALIZED 0


enum
{
    kVoltageIndex = 0,
    kCurrentIndex,
    kChargingIndex,
    kDischargingIndex,
    kRemainingCapacityIndex,
    kRunTimeToEmptyIndex,
    kACPresentIndex,
    kCapacityModeIndex,
    kNumberOfUPSElements
};

typedef struct UPSElementInfo {
    IOHIDElementCookie		cookie;
    SInt32			currentValue;
} UPSElementInfo;

typedef struct UPSDeviceData {
    UInt32			index;			// Index into our global array of UPSDeviceData
    io_object_t			notification;		// Used to dispose of the notification later
    UInt32			vendorID;		// USB VendorID
    UInt32			productID;		// USB ProductID
    UInt32			locationID;		// USB LocationID
    UInt32			primaryUsagePage;	// HID Primary Usage Page for device
    UInt32			primaryUsage;		// HID Primary Usage for device
    IOHIDDeviceInterface *	* hidDeviceInterface;	// CF Plugin for HID Manager
    IOHIDQueueInterface	*	* hidQueue;		// HID queue reference
    CFMutableDictionaryRef	upsDictRef;
    SCDynamicStoreRef		upsStore;		// for Power Manager
    CFStringRef			upsStoreKey;
    CFStringRef			nameStr;
    UPSElementInfo		elementInfo[kNumberOfUPSElements];
    bool			isPresent;
} UPSDeviceData;


enum
{
    kMaxPowerDevices = 10
};

//================================================================================================
//   Globals
//================================================================================================
//
static IONotificationPortRef	gNotifyPort;				// 
static io_iterator_t		gAddedIter;				// 
static UPSDeviceData *		gUPSDataRef[kMaxPowerDevices];		// Private Data

//================================================================================================
//   Utility routines for managing our UPS Data Refs
//================================================================================================
//


//================================================================================================
//
//	CreatePowerManagerUPSEntry
//
//	Returns error. If we can't setup connection with Power Manager, there is not much point
// to finishing other UPS support. Now creates the Power Manager entry with default values.
// Since we needed the code to update established Power Manager entries, we just follow this
// code with the call to UpdatePowerManagerUPSEntry and we only need the logic one place.
//
//================================================================================================
//
static kern_return_t CreatePowerManagerUPSEntry(UPSDeviceData *upsDataRef)
{
    CFMutableDictionaryRef	upsDictRef;
    SCDynamicStoreRef		upsStore;
    CFStringRef			upsStoreKey;
    kern_return_t 		status = kIOReturnSuccess;

    int elementValue = 0;
    CFNumberRef elementNumRef;
    
    char				upsLabelString[] = {'/', 'U', 'S', 'B', '_', 'U', 'P', 'S', 0, 0};
    #define kDefaultUPSName		"USB UPS"

    #if UPS_TOOL_DEBUG
    printf ("Entering CreatePowerManagerUPSEntry\n");
    #endif

    upsDictRef = CFDictionaryCreateMutable(kCFAllocatorDefault, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // Initialize kIOPSIsPresentKey. We wouldn't be creating this stuct if we hadn't just
    // gotten notice of a new device plugged in, so value must be TRUE.
    //
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSIsPresentKey), kCFBooleanTrue);

    // We also hope that since we plugged it in, that it is charging.
    //
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);

    // Initialize kIOPSTransportKey
    //
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSTransportTypeKey), CFSTR(kIOPSUSBTransportType));

    // Initialize kIOPSNameKey
    //
    // Ask the interface what product string or manufacturer string it has for unique name.
    // (For now just sticking in default.)
    //
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSNameKey), CFSTR(kDefaultUPSName));

    // Initialize kIOPSPowerSourceStateKey
    //
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSPowerSourceStateKey), CFSTR(kIOPSACPowerValue));
    

    //  The HID Usage Tables for Power Devices specify that the Capacity Mode can be set or read
    //  with the units as follows:
    //  0: maH
    //  1: mwH
    //  2: %
    //  3: Boolean support only
    //
    //  So, we need to make sure that the capacity mode is set to 2, i.e. %.  That way we can
    //  define a Max Capacity of 100% and use the current capacity as a percentage.
    //
    //  The Power Manager implicitly assumes that the clients will divide current/max capacities
    //  to get at a percentage of full capacity of the UPS.  Internal batteries actually seem
    //  to publish these capacities as absolute units (of something).  We achieve the same
    //  result by using percentages.
    //

    // ¥¥¥ We need to set the CapacityMode to % at this point, as all our other values depend on
    // ¥¥¥ this.
    //
    
    if ( upsDataRef->elementInfo[kRemainingCapacityIndex].cookie != 0 )
    {
        //  Initialize kIOPSCurrentCapacityKey
        //
        //  For Power Manager, we will be sharing capacity with Power Book battery capacities, so
        //  we want a consistent measure. For now we have settled on percentage of full capacity.
        //
        elementValue = 100;
        elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
        CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSCurrentCapacityKey), elementNumRef);
        CFRelease(elementNumRef);
    }


    // Initialize kIOPSMaxCapacityKey.  Since we are  using percentages, always initialize to 100%.
    //
    elementValue = 100;
    elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSMaxCapacityKey), elementNumRef);
    CFRelease(elementNumRef);

    
    if ( upsDataRef->elementInfo[kRunTimeToEmptyIndex].cookie != 0 )
    {
        // Initialize kIOPSTimeToEmptyKey (OS 9 PowerClass.c assumed 100 milliwatt-hours)
        //
        elementValue = 100;
        elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
        CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSTimeToEmptyKey), elementNumRef);
        CFRelease(elementNumRef);
    }
    
/*    
    // Initialize kIOPSTimeToFullChargeKey (OS 9 PowerClass.c assumption (in milliwatt-hours(%?))
    //
    elementValue = 0;
    elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
    CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSTimeToFullChargeKey), elementNumRef);
    CFRelease(elementNumRef);
*/

    if ( upsDataRef->elementInfo[kVoltageIndex].cookie != 0 )
    {
        // Initialize kIOPSVoltageKey (OS 9 PowerClass.c assumed millivolts. (Shouldn't that be 130,000 millivolts for AC?))
        // Actually, Power Devices Usage Tables say units will be in Volts. However we have to check what exponent is used
        // because that may make the value we get in centiVolts (exp = -2). So it looks like OS 9 sources said
        // millivolts, but used centivolts. Our final answer should device by proper exponent to get back to Volts.
        //
        elementValue = 13 * 1000 / 100;
        elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
        CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSVoltageKey), elementNumRef);
        CFRelease(elementNumRef);
    }

    if ( upsDataRef->elementInfo[kCurrentIndex].cookie != 0 )
    {
        // Initialize kIOPSCurrentKey (What would be a good amperage to initialize to?) Same discussion as for
        // Volts, where the unit for current is Amps. But with typical exponents (-2), we get centiAmps. Hmm...
        // typical current for USB may be 500 milliAmps, which would be .5 A. Since that is not an integer,
        // that may be why our displays get larger numberw
        //
        elementValue = 1;	// Just a guess!
        elementNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
        CFDictionaryAddValue(upsDictRef, CFSTR(kIOPSCurrentKey), elementNumRef);    
        CFRelease(elementNumRef);
    }

    upsStore = SCDynamicStoreCreate(NULL, CFSTR("UPS Power Manager"), NULL, NULL);

    // Uniquely name each Sys Config key
    //
    if ((upsDataRef->index > 0) && (upsDataRef->index < kMaxPowerDevices))
        upsLabelString[8] = '0' + upsDataRef->index;

    #if 0
    SCLog(TRUE, LOG_NOTICE, CFSTR("What does CreatePowerManagerUPSEntry think our key name is?"));
    SCLog(TRUE, LOG_NOTICE, CFSTR("   %@%@%@"), kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath),
        CFStringCreateWithCStringNoCopy(NULL, upsLabelString, kCFStringEncodingMacRoman, kCFAllocatorNull));
    #endif

    upsStoreKey = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@%@"), 
        kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), 
        CFStringCreateWithCStringNoCopy(NULL, upsLabelString, kCFStringEncodingMacRoman, kCFAllocatorNull));

    if(!SCDynamicStoreSetValue(upsStore, upsStoreKey, upsDictRef))
    {
        status = SCError();
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: Encountered SCDynamicStoreSetValue error 0x%x"), status);
        #endif
    }

    // Store our SystemConfiguration variables in our private data
    //
    upsDataRef->upsDictRef = upsDictRef;
    upsDataRef->upsStore = upsStore;
    upsDataRef->upsStoreKey = upsStoreKey;
    
    return status;
}

//================================================================================================
//
//	GetCurrentValueForElement()
//
//	Uses HID Manager to find latest value of an element from the UPS
//
//
//================================================================================================
//
kern_return_t	GetCurrentValueForElement( UPSDeviceData *upsDataRef, IOHIDEventStruct *upsEvent, int elementIndex )
{
    kern_return_t 	status = kIOReturnSuccess;

    #if UPS_TOOL_DEBUG
    printf ("  GetElementValueForCookie (%d)\n", elementIndex);
    #endif

    if ((upsDataRef == NULL) || (upsDataRef->hidDeviceInterface == NULL) || (upsDataRef->upsDictRef == NULL))
    {
        #if UPS_TOOL_DEBUG
        printf ("GetElementValueForCookie: some setup routine failed (%p,%p,%p)\n", upsDataRef, upsDataRef->hidDeviceInterface, upsDataRef->upsDictRef );
        #endif
        return kIOReturnNoResources;
    }

    if (upsDataRef->elementInfo[elementIndex].cookie == 0)
    {
        #if UPS_TOOL_DEBUG
        printf ("GetElementValueForCookie: No cookie for element (%d)\n", elementIndex);
        #endif
        return kIOReturnNoResources;
    }

    status = (*upsDataRef->hidDeviceInterface)->open(upsDataRef->hidDeviceInterface, 0);
    if ( status != kIOReturnSuccess )
    {
        #if UPS_TOOL_DEBUG
        printf ("GetElementValueForCookie: could not open HIDLib: 0x%x\n", status);
        #endif
        return kIOReturnNoResources;
    }

    // Query the HID Manager for this element's current value
    //
    status = (*upsDataRef->hidDeviceInterface)->getElementValue(upsDataRef->hidDeviceInterface,
                                                                upsDataRef->elementInfo[elementIndex].cookie,
                                                                upsEvent);

    if ( (*upsDataRef->hidDeviceInterface)->close(upsDataRef->hidDeviceInterface) != kIOReturnSuccess)
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: could not close IOHIDLib\n" );
        #endif
    }

    return status;
}


//================================================================================================
//
//	UpdateHIDMgrElement
//
//	Uses HID Manager to find latest value of HID elements and updates both local storage and
// updates Sys Config info.
//	This will set values in the sys config structure, but it is counting on code that follows
// this to call SCDynamicStoreSetValue to let Power Manager know about them.
//
//
//================================================================================================
//
static void UpdateHIDMgrElement(UPSDeviceData *upsDataRef, UInt8 index)
{
    IOHIDEventStruct upsEvent;
    UInt32 elementValue;
    CFNumberRef numRef;
    kern_return_t status;

#if UPS_TOOL_DEBUG
    printf ("  UpdateHIDMgrElement %d\n", index);
#endif
    
    // Validate arguments
    //
    if (index >= kNumberOfUPSElements)
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: index out of range (%d,%d)\n", kNumberOfUPSElements, index);
        #endif
        return;
    }
    
    // Is it possible for one of the setup routines to fail before we get here.
    //
    if ((upsDataRef == NULL) || (upsDataRef->hidDeviceInterface == NULL) || (upsDataRef->upsDictRef == NULL))
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: some setup routine failed (%p,%p,%p)\n", upsDataRef, upsDataRef->hidDeviceInterface, upsDataRef->upsDictRef );
        #endif
        return;
    }
    
    // Can only update HID elements that have valid cookies
    //
    if (upsDataRef->elementInfo[index].cookie == 0)
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: element %d does not have a valid cookie\n", index );
        #endif
        return; 
    }

    // Note: At the current time, when we getElementValue, if there has been no report to set the actual value,
    // HID Manager will report 0. In PowerClass.c from OS 9, we solved this problem by doing a getReport on each
    // report that had an element we are interested in. At this time, HID Manager did not have a way to get the
    // report number that the element was found in nor the ability to call getReport. So we stick with the call
    // to getElementValue. In the same time frame that those other problems should be solved, HID Manager will
    // also test to see if the requested element has not been initialized, it will go ahead and make the getReport
    // call behind our back.
    
    status = (*upsDataRef->hidDeviceInterface)->open(upsDataRef->hidDeviceInterface, 0);
    if ( status != kIOReturnSuccess )
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: could not open HIDLib: 0x%x\n", status);
        #endif
        return;
    }
    
    status = (*upsDataRef->hidDeviceInterface)->getElementValue(upsDataRef->hidDeviceInterface, 
                                                    upsDataRef->elementInfo[index].cookie,
                                                    &upsEvent);
    if (!status)
    {
        // But we tell System Config different things dependiing upon what type of HID element it is.
        elementValue = upsEvent.value;

        switch (index)
        {
            case kVoltageIndex:
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kVoltageIndex: %ld\n", elementValue);
                #endif
                numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
                CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSVoltageKey), numRef);
                CFRelease(numRef);
                break;
 
            case kCurrentIndex:
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kCurrentIndex: %ld\n", elementValue);
                #endif
                numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
                CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSCurrentKey), numRef);
                CFRelease(numRef);
                break;
 
            case kRemainingCapacityIndex:
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kRemainingCapacityIndex: %ld\n", elementValue);
                #endif
                numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
                CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSCurrentCapacityKey), numRef);
                CFRelease(numRef);
                break;

            case kRunTimeToEmptyIndex:
                // The spec says that this value is in minutes.  However, it also states that the
                // units for Time values are in seconds.  We could check the unit and unitExponent
                // to make sure that they are correct, but the only valid value is secs, so why bother.
                //
                elementValue = elementValue / 60;
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kRunTimeToEmptyIndex: %ld\n", elementValue);
                #endif
                numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
                CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSTimeToEmptyKey), numRef);
                CFRelease(numRef);
                break;

            // Experiance says that UPSes vary in which messages they send. They normally send the
            // AC Present message when going from battery to AC. However, i have seen cases where
            // there is no ACPresent == false messages to indicate we are on battery power. In that
            // case, we have to infer from getting a discharging message that we must also be on
            // battery power at that time.
            //
            case kDischargingIndex:
                // Since we want to fall through to acPresent, just reverse discharging flag and 
                // fall through to charging case first.
                //
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kDischargingIndex: %ld\n", elementValue);
                #endif
                elementValue = (elementValue) ? FALSE : TRUE;
                //break;
                //	Fall through to charging case to do actual set.

            case kChargingIndex:
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kChargingIndex: %ld\n", elementValue);
                #endif
                if (elementValue)
                    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
                else
                    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSIsChargingKey), kCFBooleanFalse);
                
                upsDataRef->elementInfo[kChargingIndex].currentValue = elementValue;
                upsDataRef->elementInfo[kDischargingIndex].currentValue = (elementValue) ? FALSE : TRUE;
                
                //break;
                //	Fall through to acPresent case to also set it.
 
            case kACPresentIndex:
                #if UPS_TOOL_DEBUG
                printf ("UpdateHIDMgrElement: kACPresentIndex: %ld\n", elementValue);
                #endif
                if (elementValue)
                    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSPowerSourceStateKey), CFSTR(kIOPSACPowerValue));
                else
                    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSPowerSourceStateKey), CFSTR(kIOPSBatteryPowerValue));
                
                upsDataRef->elementInfo[kACPresentIndex].currentValue = elementValue;
                
                // Don't fall through to reseting a currentValue.
                //
                return;
        }

        upsDataRef->elementInfo[index].currentValue = elementValue;
    }
    else
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: getElementValue for %d returned 0x%x\n", index, status );
        #endif
    }
    status = (*upsDataRef->hidDeviceInterface)->close(upsDataRef->hidDeviceInterface);
    if ( status != kIOReturnSuccess )
    {
        #if UPS_TOOL_DEBUG
        printf ("UpdateHIDMgrElement: could not close IOHIDLib\n" );
        #endif
        return;
    }
}


//================================================================================================
//
//	StorePowerCookies
//
//	By the time we get here, we are looking at a single element. If the element is one of the 
// 	ones we want, we store it's cookie in the appropriate location in the UPSDeviceData. Also,
// 	note that we are only interested in watching for input type reports. If we have any errors,
// 	we just bail without adding entries to the private data references.
//
//================================================================================================
//
static void StorePowerCookies(long type, long usagePage, long usage, IOHIDElementCookie cookie, UPSDeviceData *upsDataRef)
{
    if (type < kIOHIDElementTypeInput_Misc || type > kIOHIDElementTypeInput_ScanCodes)
    {
        // Only interested in input values!");
        return;
    }

    // Check for Power Manager usages
    //
    if ( usagePage == kHIDPage_PowerDevice )
    {
        switch (usage)
        {
            case kHIDUsage_PD_Voltage:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found voltage cookie"));
                #endif
                upsDataRef->elementInfo[kVoltageIndex].cookie = cookie;
                break;
            case kHIDUsage_PD_Current:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found current cookie"));
                #endif
                upsDataRef->elementInfo[kCurrentIndex].cookie = cookie;
                break;
        }
    }
    else if (usagePage == kHIDPage_BatterySystem)
    {
        switch (usage)
        {
            case kHIDUsage_BS_Charging:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_Charging cookie"));
                #endif
                upsDataRef->elementInfo[kChargingIndex].cookie = cookie;
                break;
            case kHIDUsage_BS_Discharging:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_Discharging cookie"));
                #endif
                upsDataRef->elementInfo[kDischargingIndex].cookie = cookie;
                break;
            case kHIDUsage_BS_RemainingCapacity:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_RemainingCapacity cookie"));
                #endif
                upsDataRef->elementInfo[kRemainingCapacityIndex].cookie = cookie;
                break;
            case kHIDUsage_BS_RunTimeToEmpty:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_RunTimeToEmpty cookie"));
                #endif
                upsDataRef->elementInfo[kRunTimeToEmptyIndex].cookie = cookie;
                break;
            case kHIDUsage_BS_ACPresent:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_ACPresent cookie"));
                #endif
                upsDataRef->elementInfo[kACPresentIndex].cookie = cookie;
                break;
            case kHIDUsage_BS_CapacityMode:
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: found kHIDUsage_BS_CapacityMode cookie"));
                #endif
                upsDataRef->elementInfo[kCapacityModeIndex].cookie = cookie;
                break;
        }
    }
}


//================================================================================================
//
//	UPSDictionaryHandler
//
//	It looks like all "Element" properties are arrays of dictionaries. So when we iterate through them,
// 	CFArrayApplyFunction should be passing dictionary elements to our handler. If the dictionary is itself
// 	a collection, we need to get the Elements of that dictionary to expand. If it is not a collection, we
// 	can test to see if it is a dictionary of attributes for a single usage.
//
//================================================================================================
//

static void UPSDictionaryHandler(const void * value, void * refcon)
{
    CFTypeRef			refCF = 0;
    IOHIDElementCookie  cookie = 0;
    long                number = 0;
    long                type = 0;
    long                usage = 0;
    long                usagePage = 0;

    // I did say that we were only coming here with dictionaries.
    if (CFGetTypeID(value) != CFDictionaryGetTypeID()) 
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: No dictionary for UPSDictionaryHandler"));
        #endif
        return;
    }

    // Get cookie for the HID Element.
    //
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementCookieKey));
    if ( (refCF != 0) && (CFGetTypeID(refCF) == CFNumberGetTypeID()) )
        if ( CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &number))
            cookie = (IOHIDElementCookie)number;

    // Get usage
    //
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsageKey));
    if ( (refCF != 0) && (CFGetTypeID(refCF) == CFNumberGetTypeID()) )
        CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &usage);

    // Get usage page
    //
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementUsagePageKey));
    if ( (refCF != 0) && (CFGetTypeID(refCF) == CFNumberGetTypeID()) )
        CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &usagePage);

    // Get HID Element type.
    //
    refCF = CFDictionaryGetValue(value, CFSTR(kIOHIDElementTypeKey));
    if ( (refCF != 0) && (CFGetTypeID(refCF) == CFNumberGetTypeID()) )
        CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &type);

    // If this is a collection, we will have to get the collection's Elements and process them recursively
    //
    if (kIOHIDElementTypeCollection == type)
    {
        CFTypeRef 	elementRef;
        
        elementRef = CFDictionaryGetValue(value, CFSTR(kIOHIDElementKey));
        
        if (elementRef)
        {
            // elementRef points to an array of dictionaries
            //
            CFRange 	range = { 0, CFArrayGetCount(elementRef) };
            
            CFArrayApplyFunction (elementRef, range, UPSDictionaryHandler, refcon);
        }
    }
    else
    {
        // It's a single element! Go and check to see if it's one of the elements
        // that we care about and store the cookie
        //
        StorePowerCookies(type, usagePage, usage, cookie, refcon);
    }
}


//================================================================================================
//
//	FindUPSCookies
//
//	We need to add the element cookies that match the usages that Power Manager is intereseted in. 
//	First we have to check the parsed descriptor that is in the IORegistry.  This consists of 
// 	possible dictionaries of dictionaries that eventually contain single elements.  These single
//	elements are what we are interested in, so we parse the dictionaries recursively until we
//	find a single element and then we look at it and decide if it's the one we want.
//
//================================================================================================
//
static void FindUPSCookies(CFMutableDictionaryRef properties, UPSDeviceData *upsDataRef)
{
    // Start at the top level ...
    //
    CFTypeRef refCFTopElement = 0;
    int i;
    
    // ... with no preexisting cookies
    //
    for (i = 0; i < kNumberOfUPSElements; i++)
        upsDataRef->elementInfo[i].cookie = 0;
    
    refCFTopElement = CFDictionaryGetValue(properties, CFSTR(kIOHIDElementKey));
    
    if (refCFTopElement)
    {
        // refCFTopElement points to an array of dictionaries
        //
        CFRange range = { 0, CFArrayGetCount(refCFTopElement) };
        
        CFArrayApplyFunction (refCFTopElement, range, UPSDictionaryHandler, upsDataRef /*refCon*/);
    }
}


//================================================================================================
//
//	UPSPollingTimer
//
//	When this routine fires, we will look at our HID event queue and see if there is any data
// 	from any of the USB UPS's in the system.  If there is, we will send messages to the Power
//	Manager with that data.
//
//================================================================================================
//
static void UPSPollingTimer(CFRunLoopTimerRef timer, void *info)
{
    IOHIDEventStruct 	event;
    IOHIDEventStruct 	hidEvent;
    AbsoluteTime 	zeroTime = {0,0};
    HRESULT 		result;
    UInt8		i;
    SInt32		minutes;
    CFNumberRef		numRef;
    Boolean		update;
    Boolean		updateIsCharging;
    Boolean		newIsCharging;
    IOReturn		status;

    // For each UPS, look at the HID queue and see if we have any information in it
    //
    for ( i = 0; i < kMaxPowerDevices; i++ )
    {
        if ( gUPSDataRef[i] != NULL && gUPSDataRef[i]->hidQueue != NULL)
        {
            while ( (result = (*(gUPSDataRef[i]->hidQueue))->getNextEvent(gUPSDataRef[i]->hidQueue, &event, zeroTime, 0)) != kIOReturnUnderrun)
            {
                update = FALSE;
                updateIsCharging = FALSE;
                newIsCharging = FALSE;

                if (result != kIOReturnSuccess)
                {
                     #if UPS_DEBUG
                    SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: queue getNextEvent result error: 0x%lx"),result);
                    #endif
                }
                else
                {                    
                    // Try to put the data that UPSes put out most frequently at the top of comparisons.
                    //
                    #if UPS_DEBUG
                    SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: queue: event:[0x%lx] %ld"),(unsigned long) event.elementCookie, event.value);
                    #endif
                    
                    #if UPS_TOOL_DEBUG
                    printf ("queue: event:[0x%lx] %ld\n", (unsigned long) event.elementCookie, event.value);
                    #endif
        
                    // Remaining Capacity
                    //
                    if (event.elementCookie == gUPSDataRef[i]->elementInfo[kRemainingCapacityIndex].cookie)
                    { 
                        #if UPS_TOOL_DEBUG
                        printf ("  Remaining Capacity (%ld, %ld, %ld)\n", event.value, gUPSDataRef[i]->elementInfo[kRemainingCapacityIndex].currentValue, gUPSDataRef[i]->elementInfo[kChargingIndex].currentValue );
                        #endif
                            
                        // Only update if it's a new value.
                        if (gUPSDataRef[i]->elementInfo[kRemainingCapacityIndex].currentValue != event.value)
                        {
                            gUPSDataRef[i]->elementInfo[kRemainingCapacityIndex].currentValue = event.value;
                            
                            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &event.value);
                            CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSCurrentCapacityKey), numRef);
                            update = TRUE;
                            CFRelease( numRef );

                            // Get the Run Time To Empty value when we update remaining capacity
                            //
                            status = GetCurrentValueForElement( gUPSDataRef[i], &hidEvent, kRunTimeToEmptyIndex);
                            if ( status == kIOReturnSuccess )
                            {
                                #if UPS_TOOL_DEBUG
                                printf("  Got new value for kRunTimeToEmpty after capacity changed (%ld)\n", hidEvent.value);
                                #endif

                                // Only update if it's a new value. Note that we change the value from secs to minutes, and since it's integer arithmetic, we
                                // drop the remainder
                                //
                                minutes = hidEvent.value / 60;

                                if (gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].currentValue != minutes)
                                {
#if UPS_TOOL_DEBUG
                                    printf("  Updating kRunTimeToEmpty after capacity changed (%ld)\n", minutes);
#endif
                                    gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].currentValue = minutes;

                                    numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                                    CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSTimeToEmptyKey), numRef);
                                    CFRelease( numRef );
                                }
                            }
                            
                        }
                    }
                   
                    // Run Time To Empty
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  Run Time to Empty (%ld, %ld, %ld)\n", event.value, gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].currentValue, gUPSDataRef[i]->elementInfo[kChargingIndex].currentValue );
                        #endif
                        
                        // Only update if it's a new value. Note that we change the value from secs to minutes, and since it's integer arithmetic, we
                        // drop the remainder
                        //
                        minutes = event.value / 60;
                        
                        if (gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].currentValue != minutes)
                        {                            
                            gUPSDataRef[i]->elementInfo[kRunTimeToEmptyIndex].currentValue = minutes;
                            
                            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &minutes);
                            CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSTimeToEmptyKey), numRef);
                            update = TRUE;
                            CFRelease( numRef );
                        }
                    }
                    
                    // Voltage
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kVoltageIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  Voltage\n");
                        #endif
                        
                        // Only update if it's a new value.
                        if (gUPSDataRef[i]->elementInfo[kVoltageIndex].currentValue != event.value)
                        {
                            gUPSDataRef[i]->elementInfo[kVoltageIndex].currentValue = event.value;
                            
                            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &event.value);
                            CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSVoltageKey), numRef);
                            update = TRUE;
                            CFRelease( numRef );
                        }
                    }
                    
                    // Current
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kCurrentIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  Current\n");
                        #endif
                        
                        // Only update if it's a new value.
                        if (gUPSDataRef[i]->elementInfo[kCurrentIndex].currentValue != event.value)
                        {
                            gUPSDataRef[i]->elementInfo[kCurrentIndex].currentValue = event.value;
                            
                            numRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &event.value);
                            CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSCurrentKey), numRef);
                            update = TRUE;
                            CFRelease( numRef );
                        }
                    }
                    
                    // AC Present
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kACPresentIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  AC Present\n");
                        #endif
                        
                        // Only update if it's a new value.
                        if (gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue != event.value)
                        {
                            gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue = event.value;
                            
                            if (event.value)
                                CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSPowerSourceStateKey),
                                                    CFSTR(kIOPSACPowerValue));
                            else
                                CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSPowerSourceStateKey), 
                                                    CFSTR(kIOPSBatteryPowerValue));

                            update = TRUE;
                        }
                    }

                    // IsCharging is now a separate key, but can be set by two different messages.
                    //
                    // Discharging
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kDischargingIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  Discharging\n");
                        #endif
                        
                        // If we get either charging or discharging, assume it is a change. (Because
                        // otherwise we would have to have some state info with relative times each message
                        // came in).
                        
                        // We only set Is Charging, so pass on to end of compares.
                        //
                        updateIsCharging = TRUE;
                        newIsCharging = event.value ? FALSE : TRUE;
                    }
                    
                    // Charging
                    //
                    else if (event.elementCookie == gUPSDataRef[i]->elementInfo[kChargingIndex].cookie)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("  Charging\n");
                        #endif
                        
                        // We only set Is Charging, so pass on to end of compares.
                        //
                        updateIsCharging = TRUE;
                        newIsCharging = event.value ? TRUE : FALSE;
                    }
        
                    // Should we update Is Charging dictionary?
                    //
                    // Experiance says that UPSes vary in which messages they send. They normally send the
                    // AC Present message when going from battery to AC. However, i have seen cases where
                    // there is no ACPresent == false messages to indicate we are on battery power. In that
                    // case, we have to infer from getting a discharging message that we must also be on
                    // battery power at that time.  We also should infer that if the remaining capacity is
                    // going down OR the time to empty is going down that we are discharging and our we are
                    // on battery
                    //
                    if (updateIsCharging)
                    {
                        if (newIsCharging)
                        {
                           CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSIsChargingKey),
                                                kCFBooleanTrue);
                            gUPSDataRef[i]->elementInfo[kChargingIndex].currentValue = TRUE;
                            gUPSDataRef[i]->elementInfo[kDischargingIndex].currentValue = FALSE;

                            if (gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue != TRUE)
                            {
                                #if UPS_TOOL_DEBUG
                                printf ("  Updating dictionary to AC Present\n");
                                #endif
                                gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue = TRUE;
                                CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSPowerSourceStateKey),
                                                        CFSTR(kIOPSACPowerValue));
                            }
                        }
                        else
                        {
                            CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSIsChargingKey), 
                                                kCFBooleanFalse);
                            gUPSDataRef[i]->elementInfo[kChargingIndex].currentValue = FALSE;
                            gUPSDataRef[i]->elementInfo[kDischargingIndex].currentValue = TRUE;

                            if (gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue != FALSE)
                            {
                                #if UPS_TOOL_DEBUG
                                printf ("  Updating dictionary to Battery Power\n");
                                #endif
                                gUPSDataRef[i]->elementInfo[kACPresentIndex].currentValue = FALSE;
                                CFDictionarySetValue(gUPSDataRef[i]->upsDictRef, CFSTR(kIOPSPowerSourceStateKey),
                                                        CFSTR(kIOPSBatteryPowerValue));
                            }
                        }
                        update = TRUE;
                    }
                    
                    // If we changed the dictionaries, tell the dynamic store.
                    //
                    if (update)
                    {
                        #if UPS_TOOL_DEBUG
                        printf ("calling SCDynamicStoreSetValue\n");
                        #endif
                        SCDynamicStoreSetValue(gUPSDataRef[i]->upsStore, gUPSDataRef[i]->upsStoreKey, 
                                                gUPSDataRef[i]->upsDictRef);
                    }  // if update
                }  // if kIOReturnSucces
            }  //  while getNextEvent
        }  // if gUPSDataRef[i] != NULL
    }  // for i < kMaxPowerDevices
}

//================================================================================================
//
//	InitializeUPSTimer
//
// 	Sets up the CFTimer that will read UPS settings every x seconds
//
//================================================================================================
//
static void InitializeUPSTimer()
{
    CFRunLoopTimerRef		upsTimer;

    upsTimer = CFRunLoopTimerCreate(NULL,
                                    CFAbsoluteTimeGetCurrent(), 		// fire date
                                    (CFTimeInterval)5.0,			// interval (kUPSPollingInterval)
                                    NULL, 0, UPSPollingTimer, NULL);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), upsTimer, kCFRunLoopDefaultMode);

    CFRelease(upsTimer);
}


//================================================================================================
//
//	DeviceNotification
//
//	This routine will get called whenever any kIOGeneralInterest notification happens.  We are
//	interested in the kIOMessageServiceIsTerminated message so that's what we look for.  Other
//	messages are defined in IOMessage.h.
//
//================================================================================================
//
void DeviceNotification( void *		refCon,
                         io_service_t 	service,
                         natural_t 	messageType,
                         void *		messageArgument )
{
    kern_return_t	kr;
    UPSDeviceData	*upsDataRef = (UPSDeviceData *) refCon;
    
    if ( (messageType == kIOMessageServiceIsTerminated) && (upsDataRef != NULL) )
    {
        // Dump our private data to stdout just to see what it looks like.
        //
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: Device (%ld/%ld) at location 0x%lx was removed"), upsDataRef->vendorID,upsDataRef->productID,upsDataRef->locationID);
        #endif

        #if UPS_TOOL_DEBUG
        printf("UPSSupport: Device (%ld/%ld) at location 0x%lx was removed\n", upsDataRef->vendorID,upsDataRef->productID,upsDataRef->locationID);
        #endif

        // Free the data we're no longer using now that the device is going away
        // Stop and dispose of the HID queue
        //
        if (upsDataRef->hidQueue != NULL)
        {
            kr = (*upsDataRef->hidQueue)->stop(upsDataRef->hidQueue);
            kr = (*upsDataRef->hidQueue)->dispose(upsDataRef->hidQueue);
            kr = (*upsDataRef->hidQueue)->Release(upsDataRef->hidQueue);
            upsDataRef->hidQueue = NULL;
        }

        // Free the HIDDeviceInterface
        //
        if (upsDataRef->hidDeviceInterface != NULL)
        {
            kr = (*upsDataRef->hidDeviceInterface)->Release (upsDataRef->hidDeviceInterface);
            upsDataRef->hidDeviceInterface = NULL;
        }
        if (upsDataRef->notification != NULL)
        {
            kr = IOObjectRelease(upsDataRef->notification);
            upsDataRef->notification = NULL;
        }
        upsDataRef->locationID = 0;
        
        // We no longer delete the sys config entry, but just tell everyone it is off line
        //
        CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSIsPresentKey), kCFBooleanFalse);
        upsDataRef->isPresent = FALSE;

        // Let Power Manager know we made the changes. It will be notified by this.
        //
        SCDynamicStoreSetValue(upsDataRef->upsStore, upsDataRef->upsStoreKey, 
                    upsDataRef->upsDictRef);
    }
}


//================================================================================================
//
//	AddUPSElementsToHIDQueue
//
//	This routine will look to see which cookies we have and add them to the HID queue so that
//	we can retrieve them later
//
//================================================================================================
//
static void AddUPSElementsToHIDQueue( UPSDeviceData *upsDataRef )
{
    kern_return_t		kr;

    // Add the elements to the queue
    //
    if (  upsDataRef->elementInfo[kVoltageIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kVoltageIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add voltageCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kCurrentIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kCurrentIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add currentCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kChargingIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kChargingIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add chargingCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kDischargingIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kDischargingIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add dischargingCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kRemainingCapacityIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kRemainingCapacityIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add remainingCapacityCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kRunTimeToEmptyIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kRunTimeToEmptyIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add runTimeToEmptyCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
    if (  upsDataRef->elementInfo[kACPresentIndex].cookie != 0 )
    {
        kr = (*upsDataRef->hidQueue)->addElement (upsDataRef->hidQueue, upsDataRef->elementInfo[kACPresentIndex].cookie, 0);
        if ( KERN_SUCCESS != kr )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't add acPresentCookie to HID queue (0x%08x)"), kr);
            #endif
        }
    }
    
}


//================================================================================================
//
//	SetupQueueForUPSReports
//
//	This is the heart of our communication with the HID Manager.  We create the CFPlugin to the
//	HID manager and set up the queue that we will check to see if there is new data from our
//	UPS. (CFPlugin moved out because we needed it earlier.)
//
//	We also register with IOKit so that we can know when our device goes away so that we can clean
//	up after ourselves.
//
//================================================================================================
//
kern_return_t SetupQueueForUPSReports( io_object_t hidDevice, UPSDeviceData * upsDataRef )
{
    kern_return_t		kr;
    kern_return_t		localErr;

    // OK, now that we have the device interface for the HID, we can use IOHIDLib to look at elements, find the
    // ones we are interested in, and set up a queue.  Then we get set up a timer so that we can look at the
    // queue and process it until it's empty.
    //
    // NOTE:  Need to release resources in case of errors here
    //
    upsDataRef->hidQueue = (*upsDataRef->hidDeviceInterface)->allocQueue (upsDataRef->hidDeviceInterface);
    if (upsDataRef->hidQueue == NULL)
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't allocate a HID queue"));
        #endif
        upsDataRef->hidQueue = NULL;
        kr = E_OUTOFMEMORY;
        goto ErrorExit;
    }

    // Create the queue
    //
    kr = (*upsDataRef->hidQueue)->create (upsDataRef->hidQueue, 0,  8);
    if ( KERN_SUCCESS != kr )
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't create a HID queue"));
        #endif
        
        goto ErrorExit;
    }

    // Add the elements to the queue
    //
    AddUPSElementsToHIDQueue( upsDataRef );
            
    // Start data delivery to queue
    //
    kr = (*upsDataRef->hidQueue)->start (upsDataRef->hidQueue);
    if ( KERN_SUCCESS != kr )
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't start a HID queue"));
        #endif
        
        goto ErrorExit;
    }
        // Register for an interest notification for this device. Pass the reference to our
    // private data as the refCon for the notification.  This will allow us to get notified
    // when this particular device goes away, so that we can clean up after ourselves.
    //
    kr = IOServiceAddInterestNotification(	gNotifyPort,		// notifyPort
                                           hidDevice,			// service
                                           kIOGeneralInterest,		// interestType
                                           DeviceNotification,		// callback
                                           upsDataRef,			// refCon
                                           &(upsDataRef->notification)	// notification
                                           );
    if (KERN_SUCCESS != kr)
    {
        // Should we return an error from here and not process anything, or should we just keep
        // processing data knowing that we'll never get notified of an unplug on this device?
        //
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: IOServiceAddInterestNotification returned 0x%08x"), kr);
        #endif
    }
    
    return KERN_SUCCESS;
    
ErrorExit:

    if ( upsDataRef->notification != NULL )
    {
        // Free the HIDDeviceInterface
        // hidDeviceInterface is now created elsewhere, so i didn't delete it here.
        //
        //localErr = (*upsDataRef->hidDeviceInterface)->Release (upsDataRef->hidDeviceInterface);
        localErr = IOObjectRelease(upsDataRef->notification);
        upsDataRef->notification = NULL;
    }
    
    if ( upsDataRef->hidQueue != NULL )
    {
        // Stop and dispose of the HID queue
        // I believe we want to preserve the error that caused us to come through ErrorExit
        // so use localErr for clean up.
        //
        localErr = (*upsDataRef->hidQueue)->stop(upsDataRef->hidQueue);
        localErr = (*upsDataRef->hidQueue)->dispose(upsDataRef->hidQueue);
        localErr = (*upsDataRef->hidQueue)->Release(upsDataRef->hidQueue);
        upsDataRef->hidQueue = NULL;
    }
    
    return kr;
}


//================================================================================================
//
//	InformPowerMangerOfUPS()
//
//	This routine is called once we have set up the dictionary.  It fills up some more fields
//	and then informs the Power Manager of our presence.
//
//================================================================================================
//
void  InformPowerMangerOfUPS( UPSDeviceData * upsDataRef )
{
    // How that we have sys config storage set up, let Power Manager know we're here.
    //                                
    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSNameKey), upsDataRef->nameStr);
    
    CFDictionarySetValue(upsDataRef->upsDictRef, CFSTR(kIOPSIsPresentKey), kCFBooleanTrue);
    upsDataRef->isPresent = TRUE;

    // Let Power Manager know we made the changes. It will be notified by this.
    //
    SCDynamicStoreSetValue(upsDataRef->upsStore, upsDataRef->upsStoreKey, 
                upsDataRef->upsDictRef);
        
}


//================================================================================================
//
//	CreateCFPluginForDevice()
//
//	Creates our cf plugin for the HID device and stores it in the globals for the device
//
//================================================================================================
//
kern_return_t  CreateCFPluginForDevice( io_object_t hidDevice, UPSDeviceData * upsDataRef )
{
    kern_return_t	kr = kIOReturnSuccess;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32 		score;
    HRESULT 		result = S_FALSE;
    
    kr = IOCreatePlugInInterfaceForService(hidDevice, kIOHIDDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
                                            &plugInInterface, &score);
    if ( KERN_SUCCESS == kr )
    {
        // I have the device plugin, I need the device interface
        //
        result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), 
                                                (LPVOID)&upsDataRef->hidDeviceInterface);
        (*plugInInterface)->Release(plugInInterface);			// done with this
        
        if ( result != S_OK )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: couldn't create a device interface (%08x)"), result);
            #endif
            if ( upsDataRef->hidDeviceInterface != NULL )
            {
                // Free the HIDDeviceInterface
                //
                (*upsDataRef->hidDeviceInterface)->Release (upsDataRef->hidDeviceInterface);
                upsDataRef->hidDeviceInterface = NULL;
            }
            kr = E_OUTOFMEMORY;
        }
    }
    else
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: unable to create a plugin (0x%x)"), kr);
        #endif
    }
    
    return kr;
}


//================================================================================================
//
//	GetPrivateData
//
//	Now that UPS entries remain in the System Configuration store, we also preserve the 
//	UPSDeviceData struct that is associated with it. Before getting a null entry from the
//	gUPSDataRef that means we will have to create a new UPSDeviceData struct, we check the
//	existing ones to see if there is a matching one that we can just reactivate. If we can't
//	find an existing UPSDeviceData struct, we will create the storage that is necessary to keep 
//	track of the UPS.  We also update
//	the global array of UPSDeviceData and fill in that data ref with the values that we want to
//	track from the UPS
//
//================================================================================================
//
UPSDeviceData *	GetPrivateData( CFMutableDictionaryRef properties )
{
    UPSDeviceData	*upsDataRef = NULL;
    UInt32		deviceVendorID = 0;
    UInt32		deviceProductID = 0;
    CFNumberRef     	number; 				// (don't release) 
    CFStringRef     	upsName;
    UInt32		i = 0;
    
    // Get the device and vendor ID for this UPS so that we can see if we already have
    // an entry for it in our global data
    //
    number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDVendorIDKey ) );
    if ( number )
        CFNumberGetValue(number, kCFNumberSInt32Type, &deviceVendorID );

    number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDProductIDKey ) );
    if ( number )
        CFNumberGetValue(number, kCFNumberSInt32Type, &deviceProductID );

    // Find an empty location in our array
    //
    for ( i = 0; i < kMaxPowerDevices; i++)
    {
        if ( gUPSDataRef[i] == NULL )
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("Creating new UPSDeviceData at index %d"), i);
            #endif

            upsDataRef = malloc(sizeof(UPSDeviceData));
            if ( upsDataRef )
            {
                bzero(upsDataRef, sizeof(UPSDeviceData));
                gUPSDataRef[i] = upsDataRef;
                upsDataRef->index = i;
           }
            break;
        }
        else if (!(gUPSDataRef[i]->isPresent) && (gUPSDataRef[i]->vendorID == deviceVendorID) &&
                  (gUPSDataRef[i]->productID == deviceProductID))
        {
            #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("Reusing UPSDeviceData at index %d"), i);
            #endif

            upsDataRef = gUPSDataRef[i];
            break;
        }
    }
    
    // If we have a pointer to our global, then fill in some of the field in that structure
    //
    if ( upsDataRef != NULL )
    {
        upsDataRef->vendorID = deviceVendorID;
        upsDataRef->productID = deviceProductID;
        
        // Get the PrimaryUsagePage for this device
        //
        upsDataRef->primaryUsagePage = 0;
        number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDPrimaryUsagePageKey ) );
        if ( number )
            CFNumberGetValue(number, kCFNumberSInt32Type, &upsDataRef->primaryUsagePage );

        // Get the PrimaryUsage for this device
        //
        upsDataRef->primaryUsage = 0;
        number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDPrimaryUsageKey ) );
        if ( number )
            CFNumberGetValue(number, kCFNumberSInt32Type, &upsDataRef->primaryUsage );

        // Get the locationID for this device
        //
        upsDataRef->locationID = 0;
        number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDLocationIDKey ) );
        if ( number )
            CFNumberGetValue(number, kCFNumberSInt32Type, &upsDataRef->locationID );
    
        // We need to save a name for this device.  First, try to see if we have a USB Product Name.  If
        // that fails then use the manufacturer and if that fails, then use a generic name.  Couldn't we use
        // a serial # here?
        //
        upsName = (CFStringRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDProductKey ) );
        if ( !upsName )
            upsName = (CFStringRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDManufacturerKey ) );
        if ( !upsName )
            upsName = CFSTR( kDefaultUPSName );

        // Even though we may have the same vendorID and productID, this may be a different UPS,
        // so update the name.
        //
        upsDataRef->nameStr = upsName;
    }
    
    return upsDataRef;
}


//================================================================================================
//
//	HIDDeviceAdded
//
//	This routine is the callback for our IOServiceAddMatchingNotification.  When we get called
//	we will look at all the devices that were added and we will:
//
//	Create some private data to relate to each device
//
//	Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for this device
//	using the refCon field to store a pointer to our private data.  When we get called with
//	this interest notification, we can grab the refCon and access our private data.
//
//================================================================================================
//
static void HIDDeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_object_t 		hidDevice = NULL;
    UPSDeviceData		*upsDataRef = NULL;
    CFMutableDictionaryRef 	properties = NULL;
    CFNumberRef			number = NULL;
    UInt32			primaryUsagePage = 0;
    int 			i;
                
    #if UPS_TOOL_DEBUG
    printf ("Entering HIDDeviceAdded\n");
    #endif

    while ( (hidDevice = IOIteratorNext(iterator)) )
    {        
        // Create a CF dictionary representation of the I/O Registry entryÕs properties
        // We then need to inspect it to find the Primary Usage Page to see if it's a device
        // that we care about
        //
        kr = IORegistryEntryCreateCFProperties (hidDevice, &properties, kCFAllocatorDefault, kNilOptions);
        if ((kr == KERN_SUCCESS) && properties)
        {
            number = (CFNumberRef) CFDictionaryGetValue( properties, CFSTR( kIOHIDPrimaryUsagePageKey ) );
            if ( number )
                CFNumberGetValue(number, kCFNumberSInt32Type, &primaryUsagePage );
            
            // If we have the correct Usage Page, then create data storage and store some properties there
            //
            if (primaryUsagePage == kHIDPage_PowerDevice || primaryUsagePage == kHIDPage_BatterySystem)
            {
                #if UPS_DEBUG
                SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: HIDDevice 0x%08x added"), hidDevice);
                #endif

                // Ah, its a USB UPS!  Add some app-specific information about this device.
                // First find out if there is an inactive buffer to hold our info or else
                // create a new buffer to hold the data.
                //
                upsDataRef = GetPrivateData( properties );

                if ( upsDataRef )
                {
                    #if UPS_TOOL_DEBUG
                    printf("UPSSupport: Device added at location 0x%8.8lx\n", upsDataRef->locationID);
                    #endif

                    // Create the CF plugin for this device
                    //
                    kr = CreateCFPluginForDevice( hidDevice, upsDataRef );
                    if ( kr == kIOReturnSuccess )
                    {
                        // Put the cookies Power Manager interested in into upsDataRef.
                        //
                        FindUPSCookies(properties, upsDataRef);

                        // If we have no system config store, we have to create it.
                        //
                        if (!(upsDataRef->upsDictRef))
                        {
                            kr = CreatePowerManagerUPSEntry(upsDataRef);
                        }
                        
                        if (kr == KERN_SUCCESS)
                        {
        
                            // We either have newly intialized values in the sys config memory or what
                            // existed previously. Ask HID Manager what the current values are.
                            //
                            for (i = 0; i < kNumberOfUPSElements; i++)
                            {
                                UpdateHIDMgrElement(upsDataRef, i);
                            }
                            
                            // Now that we have all the data for the Power Manager, let it know that
                            // it's all there
                            //
                            InformPowerMangerOfUPS( upsDataRef );
                    
                            //  Go look for the desired UPS reports and start the HID queue to process them.
                            //  Power Manager Sys Config must be setup before this to accept report changes.
                            //
                            kr = SetupQueueForUPSReports( hidDevice, upsDataRef );
                        
                            if ( kr != KERN_SUCCESS )
                            {
                                #if UPS_DEBUG
                                SCLog(TRUE, LOG_NOTICE, CFSTR("SetupForUPSReports error (0x%x) so no reports for this device."), kr);
                                #endif
                            }
                        }
                        else
                        {
#if UPS_TOOL_DEBUG
                            printf("UPSSupport: Error (0x%8.8x) creating PowerManagerUPSEntry.  Probably not running as root?\n", kr);
#endif
                        }
                    }
                    else
                    {
#if UPS_TOOL_DEBUG
                        printf("UPSSupport: Error (0x%8.8x) creating plugIn for device.\n", kr);
#endif
                    }
                }
                else
                {
#if UPS_TOOL_DEBUG
                    printf("UPSSupport: No upsDataRef!\n");
#endif
                }
            }
            
            // Release the properties dictionary
            CFRelease (properties);
        }

        // Done with this io_service_t
        //
        kr = IOObjectRelease(hidDevice);
    }
}

//================================================================================================
//
//	RegisterForUSBHIDNotifications
//
//	This routine is used to call IOKit and register to be notified when a HID device is added.
//	We also process any devices that are already plugged in.
//
//================================================================================================
//
static void RegisterForUSBHIDNotifications( mach_port_t *	masterPort)
{
    CFMutableDictionaryRef 	matchingDict;
    UInt32			usagePage = kHIDPage_PowerDevice;
    kern_return_t		kr;
    CFNumberRef			refUsagePage = NULL;

    // Set up the matching criteria for the devices we're interested in.
    // We are nterested in instances of class IOUSBInterface.
    // matchingDict is consumed below (in IOServiceAddMatchingNotification)
    // so we have no leak here.
    //
    matchingDict = IOServiceMatching(kIOHIDDeviceKey);
    if (!matchingDict)
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: Can't create a kIOHIDDeviceKey matching dictionary"));
        #endif
        
        return;
    }

    // Add a key for usagePage to our matching dictionary.   NOTE:  It looks
    // like the IOHIDDevice does not implement a matching method, so we get
    // all HID devices. Leave it here in case it gets fixed.
    //
    refUsagePage = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usagePage);
   // CFDictionarySetValue(   matchingDict,
   //                         CFSTR(kIOHIDPrimaryUsagePageKey),
   //                         refUsagePage);
    CFRelease(refUsagePage);

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    //
    gNotifyPort = IONotificationPortCreate(*masterPort);
    CFRunLoopAddSource(	CFRunLoopGetCurrent(), 
                        IONotificationPortGetRunLoopSource(gNotifyPort), 
                        kCFRunLoopDefaultMode);

    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    //
    kr = IOServiceAddMatchingNotification(gNotifyPort,			// notifyPort
                                          kIOFirstMatchNotification,	// notificationType
                                          matchingDict,			// matching
                                          HIDDeviceAdded,		// callback
                                          NULL,				// refCon
                                          &gAddedIter			// notification
                                          );

    if (KERN_SUCCESS != kr)
    {
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: IOServiceAddMatchingNotification returned 0x%08x"), kr);
        #endif
    }

    // Iterate once to get already-present devices and arm the notification
    //
    HIDDeviceAdded(NULL, gAddedIter);
}

//================================================================================================
//
//	InitUPSNotifications
//
//	This routine just creates our master port for IOKit and turns around and calls the routine
//     	that will alert us when a USB HID Device is plugged in.
//
//================================================================================================
//
void InitUPSNotifications()
{
    mach_port_t 		masterPort;
    kern_return_t		kr;
    
    #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("In InitUPSNotifications"));
    #endif
    
    #if UPS_TOOL_DEBUG
        printf ("Entering InitUPSNotifications\n");
    #endif
    
    // first create a master_port for my task
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr || !masterPort)
    {
        #if UPS_DEBUG
            SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: Couldn't create a master Port(0x%x)"), kr);
        #endif
        
        return;
    }

    // Create the IOKit notifications that we need
    //
    RegisterForUSBHIDNotifications( &masterPort );
    
}


#if STAND_ALONE_TEST_TOOL
//================================================================================================
//
//	SignalHandler
//
//	This routine will get called when we interrupt the program (usually with a Ctrl-C from the
//	command line).  We clean up so that we don't leak.
//
//================================================================================================
//
static void SignalHandler(int sigraised)
{
    printf("\nInterrupted\n");

    // Clean up here
    IONotificationPortDestroy(gNotifyPort);
    if (gAddedIter)
    {
        IOObjectRelease(gAddedIter);
        gAddedIter = 0;
    }
    exit(0);
}

//================================================================================================
//	main
//================================================================================================
//
int main (int argc, const char *argv[])
{
    sig_t			oldHandler;
    
    // Make sure our global array is all NULL
    //
    bzero( gUPSDataRef, sizeof(gUPSDataRef) );
    
    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    //
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR)
        printf("Could not establish new signal handler");

    // Set up to receive IOKit notifications of HID Devices
    //
    InitUPSNotifications ();

    // Setup the timer to read data from the UPS
    //
    InitializeUPSTimer();

    // Start the run loop. Now we'll receive notifications.
    //
    CFRunLoopRun();

    // We should never get here
    //
    return 0;
}

#else

//================================================================================================
//
//	load
//
//	Main entry point for the configd plugin
//
//================================================================================================
//
void load(CFBundleRef bundle, Boolean bundleVerbose)
{
    // Make sure our global array is all NULL
    //
    bzero( gUPSDataRef, sizeof(gUPSDataRef) );
    
    // Set up to receive IOKit notifications of HID Devices
    //
    InitUPSNotifications ();

    // Setup the timer to read data from the UPS
    //
    InitializeUPSTimer();

}
#endif
