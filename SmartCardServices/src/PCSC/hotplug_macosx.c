/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : hotplug_macosx.c
	    Package: pcsc lite
      Author : Stephen M. Webb <stephenw@cryptocard.com>
      Date   : 03 Dec 2002
	    License: Copyright (C) 2002 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides a search API for hot pluggble
	             devices.
	            
********************************************************************/

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "hotplug.h"
#include "readerfactory.h"
#include "thread_generic.h"

#define PCSCLITE_HP_DROPDIR          "/usr/libexec/SmartCardServices/drivers/"
#define PCSCLITE_HP_MANUKEY_NAME     "ifdVendorID"
#define PCSCLITE_HP_PRODKEY_NAME     "ifdProductID"
#define PCSCLITE_HP_NAMEKEY_NAME     "ifdFriendlyName"
#define PCSCLITE_HP_IFACECLASSKEY_NAME    "ifdInterfaceClass"
#define PCSCLITE_HP_IFACESUBCLASSKEY_NAME "ifdInterfaceSubClass"
#define PCSCLITE_HP_IFACEPROTOCOLKEY_NAME "ifdInterfaceProtocol"
#define PCSCLITE_HP_BASE_PORT       0x200000


/*
 * Defines the type of driver in the driver vector
 */
typedef enum 
{
    PCSCLITE_HP_Proprietary      = 0,
    PCSCLITE_HP_InterfaceClass   = 1,
    // * Could accomodate more types */
} HPDriverType;



/*
 * An aggregation of useful information on a driver bundle in the
 * drop directory.
 */
typedef struct HPDriver
{
    UInt8        m_NotEOV;           /* set to 1 for any driver before the end */
    UInt8        m_initialized;      /* set to 1 on successful intialization */
    HPDriverType m_type;             /* type of the driver in this element */
    UInt32       m_vendorId;         /* unique vendor's manufacturer code */
    UInt32       m_productId;        /* manufacturer's unique product code */
    UInt8        m_class;            /* class of a non product specific driver */
    UInt8        m_subClass;         /* subClass of a non product specific driver */
    UInt8        m_protocol;         /* protocol of a non product specific driver */
    char*        m_friendlyName;     /* bundle friendly name */
    char*        m_libPath;          /* bundle's plugin library location */
} HPDriver, *HPDriverVector;

/*
 * An aggregation on information on currently active reader drivers.
 */
typedef struct HPDevice
{
    HPDriver*         m_driver;   /* driver bundle information */
    UInt32            m_address;  /* unique system address of device */
    struct HPDevice*  m_next;     /* next device in list */
} HPDevice, *HPDeviceList;

/*
 * Pointer to a list of (currently) known hotplug reader devices (and their
                                                                  * drivers).
 */
static HPDeviceList				sDeviceList			= NULL;
static IONotificationPortRef	sNotificationPort	= NULL;
static io_iterator_t			sUSBAppearedIter	= NULL;
static io_iterator_t			sUSBRemovedIter		= NULL;
static io_iterator_t			sPCCardAppearedIter	= NULL;
static io_iterator_t			sPCCardRemovedIter	= NULL;

/*
 * A callback to handle the asynchronous appearance of new devices that are
 * candidates for PCSC readers.
 */
static void
HPDeviceAppeared(void* refCon, io_iterator_t iterator)
{
    kern_return_t kret;
    io_service_t  obj;
    while ((obj = IOIteratorNext(iterator)))
    {
        kret = IOObjectRelease(obj);
    }
    
    HPSearchHotPluggables();
}

/*
 * A callback to handle the asynchronous disappearance of devices that are
 * possibly PCSC readers.
 */
static void
HPDeviceDisappeared(void* refCon, io_iterator_t iterator)
{
    kern_return_t kret;
    io_service_t  obj;
    while ((obj = IOIteratorNext(iterator)))
    {
        kret = IOObjectRelease(obj);
    }
    HPSearchHotPluggables();
}


/*
 * Creates a vector of driver bundle info structures from the hot-plug driver
 * directory.
 *
 * Returns NULL on error and a pointer to an allocated HPDriver vector on
 * success.  The caller must free the HPDriver with a call to
 * HPDriversRelease().
 */
static HPDriverVector
HPDriversGetFromDirectory(const char* driverBundlePath)
{
    HPDriverVector bundleVector = NULL;
    CFArrayRef bundleArray;
    CFStringRef driverBundlePathString; 
    driverBundlePathString = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       driverBundlePath,
                                                       kCFStringEncodingMacRoman);
    CFURLRef pluginUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                       driverBundlePathString,
                                                       kCFURLPOSIXPathStyle, TRUE);
    CFRelease(driverBundlePathString);
    if (!pluginUrl)
    {
        DebugLogA("error getting plugin directory URL");
        return bundleVector;
    }
    bundleArray = CFBundleCreateBundlesFromDirectory(kCFAllocatorDefault,
                                                     pluginUrl,
                                                     NULL);
    if (!bundleArray)
    {
        DebugLogA("error getting plugin directory bundles");
        return bundleVector;
    }
    CFRelease(pluginUrl);
    
    size_t bundleArraySize = CFArrayGetCount(bundleArray);
    // bundleArraySize + 1 <- because the last vector element is 
    // blank and is used to determine the length (m_NotEOV == 0)
    bundleVector = (HPDriver*)calloc(bundleArraySize + 1, sizeof(HPDriver));
    if (!bundleVector)
    {
        DebugLogA("memory allocation failure");
        return bundleVector;
    }
    
    int i = 0;
    for (; i < bundleArraySize; ++i)
    {
        HPDriver* driverBundle = bundleVector + i;
        // This is not the last 
        driverBundle->m_NotEOV = 1;
        CFBundleRef currBundle = (CFBundleRef)CFArrayGetValueAtIndex(bundleArray, i);
        CFDictionaryRef dict   = CFBundleGetInfoDictionary(currBundle);
        
        CFURLRef bundleUrl      = CFBundleCopyBundleURL(currBundle);
        CFStringRef bundlePath  = CFURLCopyPath(bundleUrl);
        driverBundle->m_libPath = strdup(CFStringGetCStringPtr(bundlePath,
                                                               CFStringGetSystemEncoding()));
        if (driverBundle->m_libPath == NULL)
        {
            DebugLogA("memory allocation failure");
            return bundleVector;            
        }
        UInt32 vendorId     = 0;
        UInt8  gotVendorId  = 0;
        UInt32 productId    = 0;  
        UInt8  gotProductId = 0;

        CFStringRef strValue   = (CFStringRef)CFDictionaryGetValue(dict,
                                                                   CFSTR(PCSCLITE_HP_MANUKEY_NAME));
        if (strValue)
        {
            gotVendorId = 1;
            vendorId = strtoul(CFStringGetCStringPtr(strValue,
                                                     CFStringGetSystemEncoding()),
                                                     NULL, 16);  

            strValue = (CFStringRef)CFDictionaryGetValue(dict,
                                                         CFSTR(PCSCLITE_HP_PRODKEY_NAME));
            if (strValue)
            {
                gotProductId = 1;
                productId = strtoul(CFStringGetCStringPtr(strValue,
                                                          CFStringGetSystemEncoding()),
                                                          NULL, 16);
            }
        }
        if (gotVendorId && gotProductId)
        {
            /* This is a product-specific driver */
            driverBundle->m_productId   = productId;
            driverBundle->m_vendorId    = vendorId;
            driverBundle->m_type        = PCSCLITE_HP_Proprietary;
        }
        else
        {
            /* If not a product-specific driver, it must be */
            /* an interface class-specifc driver            */
            UInt8 class;
            UInt8 subClass;
            UInt8 protocol;
            
            strValue = (CFStringRef)CFDictionaryGetValue(dict,
                                                         CFSTR(PCSCLITE_HP_IFACECLASSKEY_NAME));
            if (strValue)
            {
                class = (UInt8) strtoul(CFStringGetCStringPtr(strValue,
                                                              CFStringGetSystemEncoding()),
                                        NULL, 16);
                driverBundle->m_class     = class;
            } 
            else
            {
                DebugLogB("Malformed bundle (class absent) in driver folder: %s. Will be ignored", 
                          driverBundle->m_libPath);
                free(driverBundle->m_libPath);
                driverBundle->m_libPath = NULL;
                continue;
            }
            strValue = (CFStringRef)CFDictionaryGetValue(dict,
                                                         CFSTR(PCSCLITE_HP_IFACESUBCLASSKEY_NAME));
            if (strValue)
            {
                subClass = (UInt8) strtoul(CFStringGetCStringPtr(strValue,
                                                                 CFStringGetSystemEncoding()),
                                           NULL, 16);
                driverBundle->m_subClass  = subClass;
            }
            else
            {
                DebugLogB("Malformed bundle (subClass absent) in driver folder: %s. Will be ignored", 
                          driverBundle->m_libPath);
                free(driverBundle->m_libPath);
                driverBundle->m_libPath = NULL;
                continue;
            }
            strValue = (CFStringRef)CFDictionaryGetValue(dict,
                                                         CFSTR(PCSCLITE_HP_IFACEPROTOCOLKEY_NAME));
            if (strValue)
            {
                protocol = (UInt8) strtoul(CFStringGetCStringPtr(strValue,
                                                                 CFStringGetSystemEncoding()),
                                           NULL, 16);
                driverBundle->m_protocol  = protocol;
            }
            else
            {
                DebugLogB("Malformed bundle (protocol absent) in driver folder: %s. Will be ignored", 
                          driverBundle->m_libPath);
                free(driverBundle->m_libPath);
                driverBundle->m_libPath = NULL;
                continue;
            }
            driverBundle->m_type = PCSCLITE_HP_InterfaceClass;
        }
        strValue = (CFStringRef)CFDictionaryGetValue(dict,
                                                     CFSTR(PCSCLITE_HP_NAMEKEY_NAME));
        if (!strValue)
        {
            DebugLogB("Product friendly name absent in driver folder: %s.",
				driverBundle->m_libPath);
            driverBundle->m_friendlyName = strdup("unnamed device");
        }
        else
        {
            const char* cstr = CFStringGetCStringPtr(strValue,
                                                     CFStringGetSystemEncoding());
            driverBundle->m_friendlyName = strdup(cstr);
        }
        driverBundle->m_initialized = 1;
    }
    CFRelease(bundleArray);
    return bundleVector;
}

/*
 * Copies a driver bundle instance.
 */
static HPDriver*
HPDriverCopy(HPDriver* rhs)
{
    if (!rhs)
    {
        return NULL;
    }
    HPDriver* newDriverBundle = (HPDriver*)calloc(1, sizeof(HPDriver));
    if (!newDriverBundle)
    {
        return NULL;
    }
    
    newDriverBundle->m_initialized  = rhs->m_initialized;
    newDriverBundle->m_type         = rhs->m_type;
    newDriverBundle->m_vendorId     = rhs->m_vendorId;
    newDriverBundle->m_productId    = rhs->m_productId;
    
    newDriverBundle->m_class        = rhs->m_class;
    newDriverBundle->m_subClass     = rhs->m_subClass;
    newDriverBundle->m_friendlyName = strdup(rhs->m_friendlyName);
    newDriverBundle->m_libPath      = strdup(rhs->m_libPath);
    if (newDriverBundle->m_friendlyName == NULL)
    {
        if (newDriverBundle->m_libPath != NULL)
        {
            free(newDriverBundle->m_libPath);
        }
        free(newDriverBundle);
        return NULL;
    }
        
    if (newDriverBundle->m_libPath == NULL)
    {
        if (newDriverBundle->m_friendlyName != NULL)
        {
            free(newDriverBundle->m_friendlyName);
        }
        free(newDriverBundle);
        return NULL;
    }
    return newDriverBundle;
}

/*
 * Releases resources allocated to a driver bundle vector.
 */
static void
HPDriverRelease(HPDriver* driverBundle)
{
    if (driverBundle)
    {
        free(driverBundle->m_friendlyName);
        free(driverBundle->m_libPath);
    }
}

/*
 * Releases resources allocated to a driver bundle vector.
 */
static void
HPDriverVectorRelease(HPDriverVector driverBundleVector)
{
    if (driverBundleVector)
    {
        HPDriver* b = driverBundleVector;
        for (; b->m_initialized; ++b)
        {
            HPDriverRelease(b);
        }
        free(driverBundleVector);
    }
}

/*
 * Inserts a new reader device in the list.
 */
static HPDeviceList
HPDeviceListInsert(HPDeviceList list, HPDriver* bundle, UInt32 address)
{
    HPDevice* newReader = (HPDevice*)calloc(1, sizeof(HPDevice));
    if (!newReader)
    {
        DebugLogA("memory allocation failure");
        return list;
    }
    newReader->m_driver  = HPDriverCopy(bundle);
    newReader->m_address = address;
    newReader->m_next    = list;
    return newReader;
}

/*
 * Frees resources allocated to a HPDeviceList.
 */
static void
HPDeviceListRelease(HPDeviceList list)
{
    HPDevice* p = list;
    for (; p; p = p->m_next)
    {
        HPDriverRelease(p->m_driver);
    }
}

/*
 * Compares two driver bundle instances for equality.
 */
static int
HPDeviceEquals(HPDevice* a, HPDevice* b)
{
    int res;
    if (a->m_driver->m_type == b->m_driver->m_type)
    {
        if (a->m_driver->m_type == PCSCLITE_HP_Proprietary)
        {
            // a and b have same vendor and product id
            res = (a->m_driver->m_vendorId == b->m_driver->m_vendorId)
                && (a->m_driver->m_productId == b->m_driver->m_productId);
        }
        else
        {
            // a and b have same class
            res = (a->m_driver->m_subClass == b->m_driver->m_subClass)
                && (a->m_driver->m_class == b->m_driver->m_class);
        }
        // AND have the same address
        res = res && (a->m_address == b->m_address);
        
        return res;
    }
    return 0;
}

/*
 * Finds USB devices currently registered in the system that match any of
 * the drivers detected in the driver bundle vector.
 */
static int
HPDriversMatchUSBDevices(HPDriverVector driverBundle, HPDeviceList* readerList)
{
    CFDictionaryRef usbMatch = IOServiceMatching("IOUSBDevice");
    if (0 == usbMatch)
    {
        DebugLogA("error getting USB match from IOServiceMatching()");
        return 1;
    }
    
    io_iterator_t usbIter;
    kern_return_t kret = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                                      usbMatch,
                                                      &usbIter);
    if (kret != 0)
    {
        DebugLogA("error getting iterator from IOServiceGetMatchingServices()");
        return 1;
    }
    
    io_object_t usbDevice = 0;
    while ((usbDevice = IOIteratorNext(usbIter)))
    {
        IOCFPlugInInterface** iodev;
        SInt32                score;
        kret = IOCreatePlugInInterfaceForService(usbDevice,
                                                 kIOUSBDeviceUserClientTypeID, 
                                                 kIOCFPlugInInterfaceID,
                                                 &iodev,
                                                 &score);
        IOObjectRelease(usbDevice);
        if (kret != 0)
        {
            DebugLogA("error getting plugin interface from IOCreatePlugInInterfaceForService()");
            continue;
        }
        
        IOUSBDeviceInterface245** usbdev;
        HRESULT hres = (*iodev)->QueryInterface(iodev,
                                                CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245),
                                                (LPVOID*)&usbdev);
        if (hres)
        {
            DebugLogA("error querying interface in QueryInterface()");
            IODestroyPlugInInterface ( iodev );
            continue;
        }
        
        else
		{
        
			UInt16 vendorId  = 0;
			UInt16 productId = 0;
			UInt32 usbAddress = 0;
			kret = (*usbdev)->GetDeviceVendor(usbdev, &vendorId);
			kret = (*usbdev)->GetDeviceProduct(usbdev, &productId);
			kret = (*usbdev)->GetLocationID(usbdev, &usbAddress);
			
			HPDriver* driver = driverBundle;
			int match = 0;
			for (; driver->m_NotEOV; ++driver)
			{
				if (!driver->m_initialized)
				{
					// Malformed driver, skip
					continue;
				}
				if ( (driver->m_type == PCSCLITE_HP_Proprietary)
					&& (driver->m_vendorId == vendorId)
					&& (driver->m_productId == productId))
				{
					*readerList = HPDeviceListInsert(*readerList, driver, usbAddress);
					match = 1;
				}
			}
			if (!match)
			{
				// Now try to locate Interfaces with supported classes
				// We create an interface iterator for each of the 
				// classes supported by drivers of PCSCLITE_HP_InterfaceClass
				// type.
	
				// Using IOServiceMatching(kIOUSBInterfaceClassName)
				// does not seem feasible as there does not seem to be a 
				// way to limit the search to the device we are currently 
				// analysing
	
				// Another option would be to iterate on all interfaces
				// and get the class of each of them. This is probably
				// not interesting as the list of PCSCLITE_HP_InterfaceClass
				// type of readers should only have one element (CCID)
				
				// Restart scan at the begining of the array
				driver = driverBundle;     
				// Iterate on PCSCLITE_HP_InterfaceClass driver types
				for (; driver->m_NotEOV; ++driver)
				{
					if (!driver->m_initialized)
					{
						// Malformed driver, skip
						continue;
					}
					if ( driver->m_type == PCSCLITE_HP_InterfaceClass)
					{
						// Iterate on interfaces of the current device
						IOUSBFindInterfaceRequest interfaceClassRequest;
						io_iterator_t			  interfaceIterator;
						io_service_t			  interface;
						
						interfaceClassRequest.bInterfaceClass = driver->m_class;
						interfaceClassRequest.bInterfaceSubClass = driver->m_subClass;	
						interfaceClassRequest.bInterfaceProtocol = driver->m_protocol;	
						interfaceClassRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;
						hres = (*usbdev)->CreateInterfaceIterator(usbdev, 
																  &interfaceClassRequest, 
																  &interfaceIterator);
						if (hres)
						{
							// Continue to next driver class
							continue;
						}
						
						while ( (interface = IOIteratorNext(interfaceIterator)) )
						{
							// Found a matching device
							*readerList = HPDeviceListInsert(*readerList, driver, usbAddress);
							match = 1;
							IOObjectRelease ( interface );
						}
						
						IOObjectRelease ( interfaceIterator );
						
					}
				}
				// Add another if (!match) for other driver types
			}   
			(*usbdev)->Release(usbdev);
			IODestroyPlugInInterface ( iodev );
		}
    }
    
    IOObjectRelease(usbIter);
    return 0;
}

/*
 * Finds PC Card devices currently registered in the system that match any of
 * the drivers detected in the driver bundle vector.
 */
static int
HPDriversMatchPCCardDevices(HPDriver* driverBundle, HPDeviceList* readerList)
{
    CFDictionaryRef pccMatch = IOServiceMatching("IOPCCard16Device");
    if (0 == pccMatch)
    {
        DebugLogA("error getting PCCard match from IOServiceMatching()");
        return 1;
    }
    
    io_iterator_t pccIter;
    kern_return_t kret = IOServiceGetMatchingServices(kIOMasterPortDefault, pccMatch, &pccIter);
    if (kret != 0)
    {
        DebugLogA("error getting iterator from IOServiceGetMatchingServices()");
        return 1;
    }
    
    io_object_t pccDevice = 0;
    while ((pccDevice = IOIteratorNext(pccIter)))
    {
        
        UInt32 vendorId   = 0;
        UInt32 productId  = 0;
        UInt32 pccAddress = 0;
        CFTypeRef valueRef = IORegistryEntryCreateCFProperty(pccDevice, CFSTR("VendorID"),
                                                             kCFAllocatorDefault, 0);
        if (!valueRef)
        {
            DebugLogA("error getting vendor");
        }
        else
        {
            CFNumberGetValue((CFNumberRef)valueRef, kCFNumberSInt32Type, &vendorId);
            CFRelease ( valueRef );
        }
        valueRef = IORegistryEntryCreateCFProperty(pccDevice, CFSTR("DeviceID"),
                                                   kCFAllocatorDefault, 0);
        if (!valueRef)
        {
            DebugLogA("error getting device");
        }
        else
        {
            CFNumberGetValue((CFNumberRef)valueRef, kCFNumberSInt32Type, &productId);
            CFRelease ( valueRef );
        }
        valueRef = IORegistryEntryCreateCFProperty(pccDevice, CFSTR("SocketNumber"),
                                                   kCFAllocatorDefault, 0);
        if (!valueRef)
        {
            DebugLogA("error getting PC Card socket");
        }
        else
        {
            CFNumberGetValue((CFNumberRef)valueRef, kCFNumberSInt32Type, &pccAddress);
            CFRelease ( valueRef );
        }
        HPDriver* driver = driverBundle;
        for (; driver->m_vendorId; ++driver)
        {
            if ((driver->m_vendorId == vendorId)
                && (driver->m_productId == productId))
            {
                *readerList = HPDeviceListInsert(*readerList, driver, pccAddress);
            }
        }
        
        IOObjectRelease ( pccDevice );
        
    }
    IOObjectRelease(pccIter);
    return 0;
}


static void
HPEstablishUSBNotification()
{

    CFMutableDictionaryRef  matchingDictionary;
    IOReturn                kret;
    
    if ( sNotificationPort == NULL )
		sNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);
	
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(sNotificationPort),
                       kCFRunLoopDefaultMode);
    
    matchingDictionary = IOServiceMatching("IOUSBDevice");
    if (!matchingDictionary)
    {
        DebugLogB("IOServiceMatching() failed", 0);
    }
    matchingDictionary = (CFMutableDictionaryRef)CFRetain(matchingDictionary);
    
    kret = IOServiceAddMatchingNotification(sNotificationPort,
                                            kIOMatchedNotification,
                                            matchingDictionary,
                                            HPDeviceAppeared, NULL,
                                            &sUSBAppearedIter);
    if (kret)
    {
        DebugLogB("IOServiceAddMatchingNotification()-1 failed with code %d", kret);
    }
	
    HPDeviceAppeared(NULL, sUSBAppearedIter);
    
    kret = IOServiceAddMatchingNotification(sNotificationPort,
                                            kIOTerminatedNotification,
                                            matchingDictionary,
                                            HPDeviceDisappeared, NULL,
                                            &sUSBRemovedIter);
    if (kret)
    {
        DebugLogB("IOServiceAddMatchingNotification()-2 failed with code %d", kret);
    }
    HPDeviceDisappeared(NULL, sUSBRemovedIter);
}

static void
HPEstablishPCCardNotification()
{
	
	CFMutableDictionaryRef  matchingDictionary;
    IOReturn                kret;

	if ( sNotificationPort == NULL )
		sNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);
	
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       IONotificationPortGetRunLoopSource(sNotificationPort),
                       kCFRunLoopDefaultMode);
    
    matchingDictionary = IOServiceMatching("IOPCCard16Device");
    if (!matchingDictionary)
    {
        DebugLogB("IOServiceMatching() failed", 0);
    }
    matchingDictionary = (CFMutableDictionaryRef)CFRetain(matchingDictionary);
    
    kret = IOServiceAddMatchingNotification(sNotificationPort,
                                            kIOMatchedNotification,
                                            matchingDictionary,
                                            HPDeviceAppeared, NULL,
                                            &sPCCardAppearedIter);
    if (kret)
    {
        DebugLogB("IOServiceAddMatchingNotification()-1 failed with code %d", kret);
    }
    HPDeviceAppeared(NULL, sPCCardAppearedIter);
    
    kret = IOServiceAddMatchingNotification(sNotificationPort,
                                            kIOTerminatedNotification,
                                            matchingDictionary,
                                            HPDeviceDisappeared, NULL,
                                            &sPCCardRemovedIter);
    if (kret)
    {
        DebugLogB("IOServiceAddMatchingNotification()-2 failed with code %d", kret);
    }
    HPDeviceDisappeared(NULL, sPCCardRemovedIter);
}

/*
 * Thread runner (does not return).
 */
static void
HPDeviceNotificationThread()
{
    HPEstablishUSBNotification();
    HPEstablishPCCardNotification();
    CFRunLoopRun();
}

/*
 * Scans the hotplug driver directory and looks in the system for matching devices.
 * Adds or removes matching readers as necessary.
 */
LONG
HPSearchHotPluggables()
{
    HPDriver* drivers = HPDriversGetFromDirectory(PCSCLITE_HP_DROPDIR);
    if (!drivers) return 1;
    
    HPDeviceList devices = NULL;
    int istat;
    istat = HPDriversMatchUSBDevices(drivers, &devices);
    if (istat)
    {
        return -1;
    }
    istat = HPDriversMatchPCCardDevices(drivers, &devices);
    if (istat)
    {
        return -1;
    }
    
    HPDevice* a = devices;
    for (; a; a = a->m_next)
    {
        int found = 0;
        HPDevice* b = sDeviceList;
        for (; b; b = b->m_next)
        {
            if (HPDeviceEquals(a, b))
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            RFAddReader(a->m_driver->m_friendlyName,
                        PCSCLITE_HP_BASE_PORT + a->m_address,
                        a->m_driver->m_libPath);
        }
    }
    
    a = sDeviceList;
    for (; a; a = a->m_next)
    {
        int found = 0;
        HPDevice* b = devices;
        for (; b; b = b->m_next)
        {
            if (HPDeviceEquals(a, b))
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            RFRemoveReader(a->m_driver->m_friendlyName,
                           PCSCLITE_HP_BASE_PORT + a->m_address);
        }
    }
    
    HPDeviceListRelease(sDeviceList);
    sDeviceList = devices;
    HPDriverVectorRelease(drivers);
    return 0;
}


PCSCLITE_THREAD_T sHotplugWatcherThread;

/*
 * Sets up callbacks for device hotplug events.
 */
LONG
HPRegisterForHotplugEvents()
{
    LONG sstat;
    sstat = SYS_ThreadCreate(&sHotplugWatcherThread,
                             NULL,
                             (LPVOID)HPDeviceNotificationThread,
                             NULL);
    return 0;
}
