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
//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------

#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <libc.h>
#include <servers/bootstrap.h>
#include <sysexits.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>
#include <IOKit/ps/IOPSKeys.h>

#include <SystemConfiguration/SystemConfiguration.h>
#if UPS_DEBUG
    #include <SystemConfiguration/SCPrivate.h>
#endif

#include "IOUPSPlugIn.h"
#include "IOUPSPrivate.h"

#define kDefaultUPSName		"Generic UPS"

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------
static CFRunLoopSourceRef 	gClientRequestRunLoopSource = NULL;
static CFRunLoopRef		gMainRunLoop = NULL;
static CFMutableArrayRef	gUPSDataArrayRef = NULL;
static IONotificationPortRef	gNotifyPort = NULL;
static io_iterator_t		gAddedIter = MACH_PORT_NULL;

//---------------------------------------------------------------------------
// TypeDefs
//---------------------------------------------------------------------------
typedef struct UPSData
{
    io_object_t                 notification;
    IOUPSPlugInInterface ** 	upsPlugInInterface;
    int                         upsID;
    Boolean                     isPresent;
    CFMutableDictionaryRef      upsStoreDict;
    SCDynamicStoreRef           upsStore;
    CFStringRef                 upsStoreKey;
    CFRunLoopSourceRef          upsEventSource;
    CFRunLoopTimerRef           upsEventTimer;
} UPSData;

typedef UPSData * 		UPSDataRef;


//---------------------------------------------------------------------------
// Methods
//---------------------------------------------------------------------------
static void SignalHandler(int sigraised);
static void InitUPSNotifications();
static void UPSDeviceAdded(void *refCon, io_iterator_t iterator);
static void DeviceNotification(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);
static void UPSEventCallback(void * target, IOReturn result, void *refcon, void *sender, CFDictionaryRef event);
static void ProcessUPSEvent(UPSDataRef upsDataRef, CFDictionaryRef event);
static UPSDataRef GetPrivateData( CFDictionaryRef properties );
static IOReturn CreatePowerManagerUPSEntry(UPSDataRef upsDataRef, CFDictionaryRef properties, CFSetRef capabilities);
static Boolean SetupMIGServer();

//---------------------------------------------------------------------------
// main
//
//---------------------------------------------------------------------------
int main (int argc, const char * argv[]) {
    openlog("upsd", LOG_PID|LOG_NDELAY, LOG_USER);
    signal(SIGINT, SignalHandler);
    SetupMIGServer();
    InitUPSNotifications();
    CFRunLoopRun();
    
    return 0;
}

//---------------------------------------------------------------------------
// SignalHandler
//---------------------------------------------------------------------------
void SignalHandler(int sigraised)
{
    syslog(LOG_INFO, "upsd: exiting SIGINT\n");    

    // Clean up here
    IONotificationPortDestroy(gNotifyPort);
    if (gAddedIter)
    {
        IOObjectRelease(gAddedIter);
        gAddedIter = 0;
    }
    exit(0);
}


//---------------------------------------------------------------------------
// SetupMIGServer
//---------------------------------------------------------------------------
extern void upsd_mach_port_callback(
    CFMachPortRef port,
    void *msg,
    CFIndex size,
    void *info);

Boolean SetupMIGServer()
{
    Boolean 			result 		= true;
    kern_return_t 		kern_result	= KERN_SUCCESS;
    CFMachPortRef 		upsdMachPort 	= NULL;  // must release
    mach_port_t         ups_port = MACH_PORT_NULL;

    /*if (IOUPSMIGServerIsRunning(&bootstrap_port, NULL)) {
        result = false;
        goto finish;
    }*/
    
    kern_result = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
    if (kern_result != KERN_SUCCESS)
    {
        result = false;
        goto finish;
    }

    gMainRunLoop = CFRunLoopGetCurrent();
    if (!gMainRunLoop) {
        result = false;
        goto finish;
    }


    kern_result = bootstrap_check_in(
                        bootstrap_port,
                        kIOUPSPlugInServerName, 
                        &ups_port);
                        
    if (BOOTSTRAP_SUCCESS != kern_result) 
    {
        syslog(LOG_ERR, "ioupsd: bootstrap_check_in \"%s\" error = %d\n",
                        kIOUPSPlugInServerName, kern_result);
    } else {
    
        upsdMachPort = CFMachPortCreateWithPort(kCFAllocatorDefault, ups_port,
                                        upsd_mach_port_callback, NULL, NULL);
        gClientRequestRunLoopSource = CFMachPortCreateRunLoopSource(
            kCFAllocatorDefault, upsdMachPort, 0);
        if (!gClientRequestRunLoopSource) {
            result = false;
            goto finish;
        }
        CFRunLoopAddSource(gMainRunLoop, gClientRequestRunLoopSource,
            kCFRunLoopDefaultMode);
    }
finish:
    if (gClientRequestRunLoopSource)  CFRelease(gClientRequestRunLoopSource);
    if (upsdMachPort)                CFRelease(upsdMachPort);

    return result;
}

//---------------------------------------------------------------------------
// InitUPSNotifications
//
// This routine just creates our master port for IOKit and turns around 
// and calls the routine that will alert us when a UPS Device is plugged in.
//---------------------------------------------------------------------------

void InitUPSNotifications()
{
    CFMutableDictionaryRef 	matchingDict;
    CFMutableDictionaryRef	propertyDict;
    kern_return_t		kr;

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    //
    gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopAddSource(	CFRunLoopGetCurrent(), 
                        IONotificationPortGetRunLoopSource(gNotifyPort), 
                        kCFRunLoopDefaultMode);

    // Create the IOKit notifications that we need
    //
    matchingDict = IOServiceMatching(kIOServiceClass); 
    
    if (!matchingDict)
	return;    
        
    propertyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                    0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (!propertyDict)
    {
        CFRelease(matchingDict);
	return;
    }
        
    // We are only interested in devices that have kIOUPSDeviceKey property set
    CFDictionarySetValue(propertyDict, CFSTR(kIOUPSDeviceKey), kCFBooleanTrue);
    
    CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyDict);
    
    CFRelease(propertyDict);


    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    kr = IOServiceAddMatchingNotification(gNotifyPort,			// notifyPort
                                          kIOFirstMatchNotification,	// notificationType
                                          matchingDict,			// matching
                                          UPSDeviceAdded,		// callback
                                          NULL,				// refCon
                                          &gAddedIter			// notification
                                          );

    if ( kr != kIOReturnSuccess )
        return;
        
    UPSDeviceAdded( NULL, gAddedIter );
}

//---------------------------------------------------------------------------
// UPSDeviceAdded
//
// This routine is the callback for our IOServiceAddMatchingNotification.
// When we get called we will look at all the devices that were added and 
// we will:
//
// Create some private data to relate to each device
//
// Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for 
// this device using the refCon field to store a pointer to our private data.
// When we get called with this interest notification, we can grab the refCon
// and access our private data.
//---------------------------------------------------------------------------

void UPSDeviceAdded(void *refCon, io_iterator_t iterator)
{
    io_object_t             upsDevice           = MACH_PORT_NULL;
    UPSDataRef              upsDataRef          = NULL;
    CFDictionaryRef         upsProperties       = NULL;
    CFDictionaryRef         upsEvent            = NULL;
    CFSetRef                upsCapabilites 		= NULL;
    CFRunLoopSourceRef      upsEventSource      = NULL;
    CFRunLoopTimerRef       upsEventTimer       = NULL;
    CFTypeRef               typeRef             = NULL;
    IOCFPlugInInterface **	plugInInterface 	= NULL;
    IOUPSPlugInInterface_v140 **	upsPlugInInterface 	= NULL;
    HRESULT                 result              = S_FALSE;
    IOReturn                kr;
    SInt32                  score;
        
    while ( (upsDevice = IOIteratorNext(iterator)) )
    {        
        // Create the CF plugin for this device
        kr = IOCreatePlugInInterfaceForService(upsDevice, kIOUPSPlugInTypeID, 
                    kIOCFPlugInInterfaceID, &plugInInterface, &score);
                    
        if ( kr != kIOReturnSuccess )
            goto UPSDEVICEADDED_NONPLUGIN_CLEANUP;
            
        // Grab the new v140 interface
        result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUPSPlugInInterfaceID_v140), 
                                                (LPVOID)&upsPlugInInterface);
                                                
        if ( ( result == S_OK ) && upsPlugInInterface )
        {
            kr = (*upsPlugInInterface)->createAsyncEventSource(upsPlugInInterface, &typeRef);
            
            if ((kr != kIOReturnSuccess) || !typeRef)
                goto UPSDEVICEADDED_FAIL;
                
            if ( CFGetTypeID(typeRef) == CFRunLoopTimerGetTypeID() )
            {
                upsEventTimer = (CFRunLoopTimerRef)typeRef;
                CFRunLoopAddTimer(CFRunLoopGetCurrent(), upsEventTimer, kCFRunLoopDefaultMode);
            }
            else if ( CFGetTypeID(typeRef) == CFRunLoopSourceGetTypeID() )
            {
                upsEventSource = (CFRunLoopSourceRef)typeRef;
                CFRunLoopAddSource(CFRunLoopGetCurrent(), upsEventSource, kCFRunLoopDefaultMode);
            }
        }
        // Couldn't grab the new interface.  Fallback on the old.
        else
        {
            result = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUPSPlugInInterfaceID), 
                                                (LPVOID)&upsPlugInInterface);
        }
                                                        
        // Got the interface
        if ( ( result == S_OK ) && upsPlugInInterface )
        {
            kr = (*upsPlugInInterface)->getProperties(upsPlugInInterface, &upsProperties);
            
            if (kr != kIOReturnSuccess)
                goto UPSDEVICEADDED_FAIL;
                
            upsDataRef = GetPrivateData(upsProperties);

            if ( !upsDataRef )
                goto UPSDEVICEADDED_FAIL;

            upsDataRef->upsPlugInInterface  = (IOUPSPlugInInterface **)upsPlugInInterface;
            upsDataRef->upsEventSource      = upsEventSource;
            upsDataRef->upsEventTimer       = upsEventTimer;
            upsDataRef->isPresent           = true;
            
            kr = (*upsPlugInInterface)->getCapabilities(upsPlugInInterface, &upsCapabilites);

            if (kr != kIOReturnSuccess)
                goto UPSDEVICEADDED_FAIL;

            kr = CreatePowerManagerUPSEntry(upsDataRef, upsProperties, upsCapabilites);

            if (kr != kIOReturnSuccess)
                goto UPSDEVICEADDED_FAIL;

            kr = (*upsPlugInInterface)->getEvent(upsPlugInInterface, &upsEvent);

            if (kr != kIOReturnSuccess)
                goto UPSDEVICEADDED_FAIL;

            ProcessUPSEvent(upsDataRef, upsEvent);

            (*upsPlugInInterface)->setEventCallback(upsPlugInInterface, UPSEventCallback, NULL, upsDataRef);

            IOServiceAddInterestNotification(	
                                    gNotifyPort,		// notifyPort
                                    upsDevice,			// service
                                    kIOGeneralInterest,		// interestType
                                    DeviceNotification,		// callback
                                    upsDataRef,			// refCon
                                    &(upsDataRef->notification)	// notification
                                    );
                                    
            goto UPSDEVICEADDED_CLEANUP;
        }

UPSDEVICEADDED_FAIL:
        // Failed to allocated a UPS interface.  Do some cleanup
        if ( upsPlugInInterface )
        {
            (*upsPlugInInterface)->Release(upsPlugInInterface);
            upsPlugInInterface = NULL;
        }
        
        if ( upsEventSource )
        {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), upsEventSource, kCFRunLoopDefaultMode);
            upsEventSource = NULL;
        }

        if ( upsEventTimer )
        {
            CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), upsEventTimer, kCFRunLoopDefaultMode);
            upsEventSource = NULL;
        }

UPSDEVICEADDED_CLEANUP:
        // Clean up
        (*plugInInterface)->Release(plugInInterface);
        
UPSDEVICEADDED_NONPLUGIN_CLEANUP:
        IOObjectRelease(upsDevice);
    }
}

//---------------------------------------------------------------------------
// DeviceNotification
//
// This routine will get called whenever any kIOGeneralInterest notification 
// happens. 
//---------------------------------------------------------------------------

void DeviceNotification(void *		refCon,
                        io_service_t 	service,
                        natural_t 	messageType,
                        void *		messageArgument )
{
    UPSDataRef		upsDataRef = (UPSDataRef) refCon;

    if ( (upsDataRef != NULL) &&
         (messageType == kIOMessageServiceIsTerminated) )
    {
        upsDataRef->isPresent = FALSE;
        
        SCDynamicStoreRemoveValue(upsDataRef->upsStore, upsDataRef->upsStoreKey);

        if ( upsDataRef->upsEventSource )
        {
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), upsDataRef->upsEventSource, kCFRunLoopDefaultMode);
            CFRelease(upsDataRef->upsEventSource);
            upsDataRef->upsEventSource = NULL;
        }

        if ( upsDataRef->upsEventTimer )
        {
            CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), upsDataRef->upsEventTimer, kCFRunLoopDefaultMode);
            CFRelease(upsDataRef->upsEventTimer);
            upsDataRef->upsEventTimer = NULL;
        }

        if (upsDataRef->upsPlugInInterface != NULL)
        {
            (*(upsDataRef->upsPlugInInterface))->Release (upsDataRef->upsPlugInInterface);
            upsDataRef->upsPlugInInterface = NULL;
        }
        
        if (upsDataRef->notification != MACH_PORT_NULL)
        {
            IOObjectRelease(upsDataRef->notification);
            upsDataRef->notification = MACH_PORT_NULL;
        }

        if (upsDataRef->upsStoreKey)
        {
            CFRelease(upsDataRef->upsStoreKey);
            upsDataRef->upsStoreKey = NULL;
        }

        if (upsDataRef->upsStoreDict)
        {
            CFRelease(upsDataRef->upsStoreDict);
            upsDataRef->upsStoreDict = NULL;
        }

        if (upsDataRef->upsStore)
        {
            CFRelease(upsDataRef->upsStore);
            upsDataRef->upsStore = NULL;
        }
    }
}

//---------------------------------------------------------------------------
// UPSEventCallback
//
// This routine will get called whenever any data is available from the UPS
//---------------------------------------------------------------------------
void UPSEventCallback(	void *	 		target,
                        IOReturn 		result,
                        void * 			refcon,
                        void * 			sender,
                        CFDictionaryRef		event)
{
    ProcessUPSEvent((UPSDataRef) refcon, event);
}

//---------------------------------------------------------------------------
// ProcessUPSEvent
//
//---------------------------------------------------------------------------
void ProcessUPSEvent(UPSDataRef upsDataRef, CFDictionaryRef event)
{
    UInt32		count, index;
    
    if ( !upsDataRef || !event)
        return;
      
    if ( (count = CFDictionaryGetCount(event)) )
    {	
        CFTypeRef * keys	= (CFTypeRef *) malloc(sizeof(CFTypeRef) * count);
        CFTypeRef * values	= (CFTypeRef *) malloc(sizeof(CFTypeRef) * count);

        CFDictionaryGetKeysAndValues(event, (const void **)keys, (const void **)values);
        
        for (index = 0; index < count; index++)
            CFDictionarySetValue(upsDataRef->upsStoreDict, keys[index], values[index]);
            
        free (keys);
        free (values);
            
        SCDynamicStoreSetValue(upsDataRef->upsStore, upsDataRef->upsStoreKey, upsDataRef->upsStoreDict);
    }
}


//---------------------------------------------------------------------------
// GetPrivateData
//
// Now that UPS entries remain in the System Configuration store, we also 
// preserve the UPSDeviceData struct that is associated with it. Before 
// getting a null entry from the gUPSDataRef that means we will have to 
// create a new UPSDeviceData struct, we check the existing ones to see if 
// there is a matching one that we can just reactivate. If we can't find an 
// existing UPSDeviceData struct, we will create the storage that is 
// necessary to keep track of the UPS.  We also update the global array of 
// UPSDeviceData and fill in that data ref with the values that we want to
// track from the UPS
//---------------------------------------------------------------------------

UPSDataRef GetPrivateData( CFDictionaryRef properties )
{
    UPSDataRef		upsDataRef 		= NULL;
    CFMutableDataRef	data			= NULL;
    UInt32		i 			= 0;
    UInt32		count 			= 0;
    

    // Allocated the global array if necessary
    if (!gUPSDataArrayRef && 
        !(gUPSDataArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks)))
    {
        return NULL;
    }

    // Get the device and vendor ID for this UPS so that we can see if we already have
    // an entry for it in our global data
    //

    // Find an empty location in our array
    count = CFArrayGetCount(gUPSDataArrayRef);
    for ( i = 0; i < count; i++)
    {        
        data = (CFMutableDataRef)CFArrayGetValueAtIndex(gUPSDataArrayRef, i);
        if ( !data )
            continue;
            
        upsDataRef =(UPSDataRef)CFDataGetMutableBytePtr(data);

        if (upsDataRef && !(upsDataRef->isPresent))
            break;
        
        upsDataRef = NULL;
    }
    
    // No valid upsDataRef was found, so let's go ahead and allocate one
    if ( (upsDataRef == NULL) && 
         (data = CFDataCreateMutable(kCFAllocatorDefault, sizeof(UPSData))) )
    {
        upsDataRef =(UPSDataRef)CFDataGetMutableBytePtr(data);
        bzero( upsDataRef, sizeof(UPSData) );

        CFArrayAppendValue(gUPSDataArrayRef, data);
        CFRelease(data);
    }
    
    // If we have a pointer to our global, then fill in some of the field in that structure
    //
    if ( upsDataRef != NULL )
    {
        upsDataRef->upsID	= i;
    }
    
    return upsDataRef;
}


//---------------------------------------------------------------------------
// CreatePowerManagerUPSEntry
//
//---------------------------------------------------------------------------
#define kInternalUPSLabelLength     20

IOReturn CreatePowerManagerUPSEntry(UPSDataRef upsDataRef, CFDictionaryRef properties, CFSetRef capabilities)
{
    CFMutableDictionaryRef	upsStoreDict 	= NULL;
    CFStringRef     		upsName 	= NULL;
    CFStringRef			transport	= NULL;
    CFStringRef			upsStoreKey	= NULL;
    CFNumberRef 		number 	= NULL;
    SCDynamicStoreRef		upsStore 	= NULL;
    IOReturn	 		status 		= kIOReturnSuccess;
    int 			elementValue 	= 0;
    char			upsLabelString[kInternalUPSLabelLength];

    if ( !upsDataRef || !properties || !capabilities)
        return kIOReturnError;
        
    upsStoreDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // Set some Store values
    if ( upsStoreDict )
    {
        // We need to save a name for this device.  First, try to see if we have a USB Product Name.  If
        // that fails then use the manufacturer and if that fails, then use a generic name.  Couldn't we use
        // a serial # here?
        //
        upsName = (CFStringRef) CFDictionaryGetValue( properties, CFSTR( kIOPSNameKey ) );
        if ( !upsName )
            upsName = CFSTR(kDefaultUPSName);
        transport = (CFStringRef) CFDictionaryGetValue( properties, CFSTR( kIOPSTransportTypeKey ) );

        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSNameKey), upsName);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSTransportTypeKey), transport);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSIsPresentKey), kCFBooleanTrue);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSIsChargingKey), kCFBooleanTrue);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSPowerSourceStateKey), CFSTR(kIOPSACPowerValue));
        
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &upsDataRef->upsID);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSPowerSourceIDKey), number);
        CFRelease(number);


        elementValue = 100;
        number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
        CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSMaxCapacityKey), number);
        CFRelease(number);

        if (CFSetContainsValue(capabilities, CFSTR(kIOPSCurrentCapacityKey)))
        {
            //  Initialize kIOPSCurrentCapacityKey
            //
            //  For Power Manager, we will be sharing capacity with Power Book battery capacities, so
            //  we want a consistent measure. For now we have settled on percentage of full capacity.
            //
            elementValue = 100;
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
            CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSCurrentCapacityKey), number);
            CFRelease(number);
        }

        if (CFSetContainsValue(capabilities, CFSTR(kIOPSTimeToEmptyKey)))
        {
            // Initialize kIOPSTimeToEmptyKey (OS 9 PowerClass.c assumed 100 milliwatt-hours)
            //
            elementValue = 100;
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
            CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSTimeToEmptyKey), number);
            CFRelease(number);
        }

        if (CFSetContainsValue(capabilities, CFSTR(kIOPSVoltageKey)))
        {
            // Initialize kIOPSVoltageKey (OS 9 PowerClass.c assumed millivolts. 
            // (Shouldn't that be 130,000 millivolts for AC?))
            // Actually, Power Devices Usage Tables say units will be in Volts. 
            // However we have to check what exponent is used because that may 
            // make the value we get in centiVolts (exp = -2). So it looks like 
            // OS 9 sources said millivolts, but used centivolts. Our final 
            // answer should device by proper exponent to get back to Volts.
            //
            elementValue = 13 * 1000 / 100;
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
            CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSVoltageKey), number);
            CFRelease(number);
        }

        if (CFSetContainsValue(capabilities, CFSTR(kIOPSCurrentKey)))
        {
            // Initialize kIOPSCurrentKey (What would be a good amperage to 
            // initialize to?) Same discussion as for Volts, where the unit 
            // for current is Amps. But with typical exponents (-2), we get 
            // centiAmps. Hmm... typical current for USB may be 500 milliAmps, 
            // which would be .5 A. Since that is not an integer, that may be 
            // why our displays get larger numbers
            //
            elementValue = 1;	// Just a guess!
            number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &elementValue);
            CFDictionarySetValue(upsStoreDict, CFSTR(kIOPSCurrentKey), number);    
            CFRelease(number);
        }
    }

    upsStore = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("UPS Power Manager"), NULL, NULL);

    // Uniquely name each Sys Config key
    //
    snprintf(upsLabelString, kInternalUPSLabelLength, "/UPS%d", upsDataRef->upsID);

    #if 0
    SCLog(TRUE, LOG_NOTICE, CFSTR("What does CreatePowerManagerUPSEntry think our key name is?"));
    SCLog(TRUE, LOG_NOTICE, CFSTR("   %@%@%@"), kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath),
        CFStringCreateWithCStringNoCopy(NULL, upsLabelString, kCFStringEncodingMacRoman, kCFAllocatorNull));
    #endif

    CFStringRef     upsLabelCF = NULL;
    
    upsLabelCF = CFStringCreateWithCStringNoCopy(NULL, upsLabelString, kCFStringEncodingMacRoman, kCFAllocatorNull);
    if (upsLabelCF) {
        upsStoreKey = SCDynamicStoreKeyCreate(kCFAllocatorDefault, CFSTR("%@%@%@"), 
                                              kSCDynamicStoreDomainState, CFSTR(kIOPSDynamicStorePath), upsLabelCF);
        CFRelease(upsLabelCF);
    }

    if(!upsStoreKey || !SCDynamicStoreSetValue(upsStore, upsStoreKey, upsStoreDict))
    {
        status = SCError();
        #if UPS_DEBUG
        SCLog(TRUE, LOG_NOTICE, CFSTR("UPSSupport: Encountered SCDynamicStoreSetValue error 0x%x"), status);
        #endif
    }

    if (kIOReturnSuccess == status)
    {
        // Store our SystemConfiguration variables in our private data
        //
        upsDataRef->upsStoreDict 	= upsStoreDict;
        upsDataRef->upsStore 	= upsStore;
        upsDataRef->upsStoreKey 	= upsStoreKey;
    } else {
        if (upsStoreDict)
            CFRelease(upsStoreDict);
        if (upsStore)
            CFRelease(upsStore);
        if (upsStoreKey)
            CFRelease(upsStoreKey);
    }
    return status;
}


//===========================================================================
// MIG Routines
//===========================================================================

//---------------------------------------------------------------------------
// _io_ups_send_command
//
// This routine allow remote processes to issue commands to the UPS.  It is
// expected that command will come in as a serialized CFDictionaryRef.
//---------------------------------------------------------------------------
kern_return_t _io_ups_send_command(
                mach_port_t 		server,
                int 			upsID,
                void * 			commandBuffer,
                IOByteCount		commandSize)
{
    CFDictionaryRef	command;
    CFMutableDataRef	data;
    UPSDataRef		upsDataRef;
    IOReturn		res = kIOReturnError;
        
    command = (CFDictionaryRef)IOCFUnserialize(commandBuffer, kCFAllocatorDefault, kNilOptions, NULL);
    if (command)
    {
        if (!gUPSDataArrayRef || (upsID >= CFArrayGetCount(gUPSDataArrayRef)))
        {
            res = kIOReturnBadArgument;
        }
        else
        {
            data = (CFMutableDataRef)CFArrayGetValueAtIndex(gUPSDataArrayRef, upsID);
            upsDataRef =(UPSDataRef)CFDataGetMutableBytePtr(data);
            
            if (upsDataRef && upsDataRef->upsPlugInInterface)
                res = (*upsDataRef->upsPlugInInterface)->sendCommand(upsDataRef->upsPlugInInterface, command);
        }
        CFRelease(command);
    }

    return res;
}

//---------------------------------------------------------------------------
// _io_ups_get_event
//
// This routine allow remote processes to issue commands to the UPS.  It will
// return a CFDictionaryRef that is serialized.
//---------------------------------------------------------------------------
kern_return_t _io_ups_get_event(
                mach_port_t 		server,
                int 			upsID,
                void **			eventBufferPtr,
                IOByteCount *		eventBufferSizePtr)
{
    CFDictionaryRef	event;
    CFMutableDataRef	data;
    CFDataRef		serializedData;
    UPSDataRef		upsDataRef;
    IOReturn		res = kIOReturnError;
        
    if (!eventBufferPtr || !eventBufferSizePtr ||
        !gUPSDataArrayRef || (upsID >= CFArrayGetCount(gUPSDataArrayRef)))
    {
        return kIOReturnBadArgument;
    }
    
    data = (CFMutableDataRef)CFArrayGetValueAtIndex(gUPSDataArrayRef, upsID);
    upsDataRef = (UPSDataRef)CFDataGetMutableBytePtr(data);
    
    if (!upsDataRef || !upsDataRef->upsPlugInInterface)
        return kIOReturnBadArgument;

    res = (*upsDataRef->upsPlugInInterface)->getEvent(upsDataRef->upsPlugInInterface, &event);
    
    if ((res != kIOReturnSuccess) || !event)
        return kIOReturnError;
        

    serializedData = (CFDataRef)IOCFSerialize( event, kNilOptions );
    
    if (!serializedData)
        return kIOReturnError;
        
    *eventBufferSizePtr = CFDataGetLength(serializedData);

    vm_allocate(mach_task_self(), 
            (vm_address_t *)eventBufferPtr, 
            *eventBufferSizePtr, 
            TRUE);

    if( *eventBufferPtr )
        memcpy(*eventBufferPtr, CFDataGetBytePtr(serializedData), *eventBufferSizePtr);

    CFRelease( serializedData );

    return res;
}

//---------------------------------------------------------------------------
// _io_ups_get_capabilities
//
// This routine allow remote processes to issue commands to the UPS.  It will
// return a CFSetRef that is serialized.
//---------------------------------------------------------------------------
kern_return_t _io_ups_get_capabilities(
                mach_port_t 		server,
                int 			upsID,
                void **			capabilitiesBufferPtr,
                IOByteCount *		capabilitiesBufferSizePtr)
{
    CFSetRef		capabilities;
    CFMutableDataRef	data;
    CFDataRef		serializedData;
    UPSDataRef		upsDataRef;
    IOReturn		res = kIOReturnError;
        
    if (!capabilitiesBufferPtr || !capabilitiesBufferSizePtr ||
        !gUPSDataArrayRef || (upsID >= CFArrayGetCount(gUPSDataArrayRef)))
    {
        return kIOReturnBadArgument;
    }
    
    data = (CFMutableDataRef)CFArrayGetValueAtIndex(gUPSDataArrayRef, upsID);
    upsDataRef = (UPSDataRef)CFDataGetMutableBytePtr(data);
    
    if (!upsDataRef || !upsDataRef->upsPlugInInterface)
        return kIOReturnBadArgument;

    res = (*upsDataRef->upsPlugInInterface)->getCapabilities(
                        upsDataRef->upsPlugInInterface, 
                        &capabilities);
    
    if ((res != kIOReturnSuccess) || !capabilities)
        return kIOReturnError;
        

    serializedData = (CFDataRef)IOCFSerialize( capabilities, kNilOptions );
    
    if (!serializedData)
        return kIOReturnError;
        
    *capabilitiesBufferSizePtr = CFDataGetLength(serializedData);

    vm_allocate(mach_task_self(), 
            (vm_address_t *)capabilitiesBufferPtr, 
            *capabilitiesBufferSizePtr, 
            TRUE);

    if( *capabilitiesBufferPtr )
        memcpy(*capabilitiesBufferPtr, 
                CFDataGetBytePtr(serializedData), 
                *capabilitiesBufferSizePtr);

    CFRelease( serializedData );

    return res;
}
