/*
 *  usbserial.h
 *  ifd-CCID
 *
 *  Created by JL on Mon Feb 10 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 * Initial implementation from David Corcoran (corcoran@linuxnet.com)
 * 
 */


#include <stdio.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_error.h>


#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFPlugIn.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>


#include <IOKit/IOCFPlugIn.h>

#include <assert.h>
#include "global.h"
#include <wintypes.h>
#include "pcscdefines.h"
#include "usbserial.h"
#include "Transport.h"

#include "usbserial_mosx.h"
#include "tools.h"

#define USBMAX_READERS  (PCSCLITE_MAX_CHANNELS)
//+++ Should be included from a pcscd header file
#define PCSCLITE_HP_BASE_PORT       0x200000
#define PCSCLITE_HP_IFACECLASSKEY_NAME    "ifdInterfaceClass"
#define PCSCLITE_HP_IFACESUBCLASSKEY_NAME "ifdInterfaceSubClass"
#define PCSCLITE_HP_IFACEPROTOCOLKEY_NAME "ifdInterfaceProtocol"


// Used to read the manufacturer USB strings
#define LANGUAGE_ID 0x0409
#define STRING_REQUEST 0x03


// Read time out in milliseconds (default value)
unsigned long ReadTimeOut = 60000;


static int                      iInitialized = FALSE;


static intrFace intFace[USBMAX_READERS];


// Local helper function
void ReadUSBString(IOUSBDeviceInterface245 **dev, UInt8 bIndex,
                   const char* pcHeader);


TrRv OpenUSB( DWORD lun, DWORD Channel)
{
    kern_return_t			kr;
    IOReturn                ior;
    CFMutableDictionaryRef  USBMatch = 0;
    io_iterator_t 			iter = 0;
    io_service_t            USBDevice = 0;
    io_service_t 			USBInterface = 0;
    IOCFPlugInInterface 	**ioPlugin=NULL;
    HRESULT 				res;
    SInt32                  score;
    DWORD                   rdrLun;
    UInt8                   class, subClass, protocol;
    CFNumberRef             CFclass = 0;
    CFNumberRef             CFsubClass = 0; 
    CFNumberRef             CFprotocol = 0;     
//+    CFNumberRef             CFUsbAddress = 0; 
    UInt32   				usbAddr, targetusbAddress;
    short                   iFound;
    mach_port_t             masterPort;
    UInt8                   sleepCount;
    const char*             cStringValue;
    UInt16                  i=0;

    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Entering OpenUSB");


    rdrLun = lun >> 16;

    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }

    // Parse the bundle for various information
    cStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, PCSCLITE_HP_IFACECLASSKEY_NAME);
    if ( cStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "OpenUSB error: ifdInterfaceClass not found");
        return TrRv_ERR;
    }
    class = (UInt8) strtoul(cStringValue, 0, 16);

    cStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, PCSCLITE_HP_IFACESUBCLASSKEY_NAME);
    if ( cStringValue == NULL )
    {

        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "OpenUSB warning: ifdInterfaceSubClass not found");
        return TrRv_ERR;
    }
    subClass = (UInt8) strtoul(cStringValue, 0, 16);

    
    cStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, PCSCLITE_HP_IFACEPROTOCOLKEY_NAME);
    if ( cStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "OpenUSB warning: ifdInterfaceProtocol not found");
        return TrRv_ERR;
    }
    protocol = (UInt8) strtoul(cStringValue, 0, 16);
    
    cStringValue =  ParseInfoPlist(BUNDLE_IDENTIFIER, "ifdReadTimeOut");
    if ( cStringValue == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                    "OpenUSB warning: ifdReadTimeOut not found, use default: %ld ms", 
                    ReadTimeOut);
    }
    else
    {
       ReadTimeOut = strtoul(cStringValue, 0, 10);
    }
    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                "Driver configured to detect Interface class=%02X, subClass=%02X, protocol=%02X",
                class, subClass, protocol);    
    
    iFound = FALSE;
    
    if ( iInitialized == FALSE ) {
                
        for (i=0; i < USBMAX_READERS; i++) {
            (intFace[i]).usbAddr = 0;
            (intFace[i]).dev = NULL;
            (intFace[i]).iface = NULL;
            (intFace[i]).inPipeRef = 0;
            (intFace[i]).outPipeRef = 0;
            (intFace[i]).used = 0;
            (intFace[i]).ready = 0;
            (intFace[i]).class = 0;
            (intFace[i]).subClass = 0;            
        }
    
        iInitialized = TRUE;

    }
    
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Couldn't create a master IOKit Port (0x%08X)", kr);
        return TrRv_ERR;
    }

    

    USBMatch = IOServiceMatching(kIOUSBInterfaceClassName);
    if (!USBMatch)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Can't create a USB matching dictionary");
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }
    // Compute target usb Address from Channel ID
    
    targetusbAddress = Channel - PCSCLITE_HP_BASE_PORT;
/*+
    // Locate device according to USB address
    CFUsbAddress = CFNumberCreate(kCFAllocatorDefault,
                                  kCFNumberSInt64Type,
                                  &targetusbAddress);
    if (!CFUsbAddress)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Can't create a CFNumber for usb Address");
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }
    CFDictionarySetValue(USBMatch,
                         CFSTR(kUSBDevicePropertyAddress),
                         CFUsbAddress);
    CFRelease(CFUsbAddress);
*/    

    // Prepare CFNumbers for the dictionary 
    CFclass = CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberCharType,
                             &class);
    if (!CFclass)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Can't create a CFNumber for interface class byte");
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }
    CFDictionarySetValue(USBMatch,
                         CFSTR(kUSBInterfaceClass),
                         CFclass);
    CFRelease(CFclass);
    
    
    
    CFsubClass = CFNumberCreate(kCFAllocatorDefault,
                                kCFNumberCharType,
                                &subClass);
    if (!CFsubClass)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Can't create a CFNumber for interface subclass byte");
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }
    CFDictionarySetValue(USBMatch,
                         CFSTR(kUSBInterfaceSubClass), 
                         CFsubClass);
    CFRelease(CFsubClass);


    
    CFprotocol = CFNumberCreate(kCFAllocatorDefault,
                                kCFNumberCharType,
                                &protocol);
    if (!CFprotocol)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Can't create a CFNumber for interface protocol byte");
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }
    CFDictionarySetValue(USBMatch,
                         CFSTR(kUSBInterfaceProtocol), 
                         CFprotocol);
    CFRelease(CFprotocol);

    /* Get an iterator over all matching IOService nubs */
    kr = IOServiceGetMatchingServices(masterPort, USBMatch, &iter);
    USBMatch = 0; // This was consumed by abbove call (according to USBSimple, main.c)
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Can't create a USB Service iterator (0x%08X)", kr);
        mach_port_deallocate(mach_task_self(), masterPort);
        return TrRv_ERR;
    }

    // masterPort not used any more
    mach_port_deallocate(mach_task_self(), masterPort);

    
    // We loop on all interfaces matching the triplet (class, subclass, protocol)
    // and identify the right one using the device usb address (or Location ID)
    // This would not work with USB devices exposing 2 or more CCID interfaces
    while ( (USBInterface = IOIteratorNext(iter)) )
    {
        // Get the IOServices plug-in for the interface
        kr = IOCreatePlugInInterfaceForService(USBInterface, 
                                               kIOUSBInterfaceUserClientTypeID,
                                               kIOCFPlugInInterfaceID, 
                                               &ioPlugin, &score);
        IOObjectRelease(USBInterface);	/* done with the interface object now */
        if (kr || !ioPlugin)
        {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                        "unable to create a plugin (0x%08X)", kr);
            continue;
        }
            
        /* Get the interface */
        res = (*ioPlugin)->QueryInterface(ioPlugin, 
                                          CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245),
                                          (LPVOID)&(intFace[rdrLun]).iface);
		IODestroyPlugInInterface(ioPlugin);
        if (res || !(intFace[rdrLun]).iface)
        {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                        "Couldn't create a device interface (0x%08X)", res);
            continue;
        }
        
        // Get the USB address of the discovered matching interface
        ior = (*(intFace[rdrLun]).iface)->GetLocationID(((intFace[rdrLun]).iface), &usbAddr);
        if (ior)
        {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Couldn't get the interface location ID (0x%08X)", ior);
            continue;
        }
        LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, 
                    "Found Interface at usb address 0x%08X (target=0x%08X)", 
                    usbAddr, targetusbAddress);

        // Check if this interface is on the device given as channel ID
        if (usbAddr == targetusbAddress)
        {
            iFound = TRUE;
            break;
        }
    }
    
    
    
    // iterator not needed anymore.
    IOObjectRelease(iter);
    
    if (!iFound) 
    {
        /* Device not found */
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Unable to locate interface");
        return TrRv_ERR;
    }
    
    // Device discovered, memorize its properties
    (intFace[rdrLun]).class = class;
    (intFace[rdrLun]).subClass = subClass;
    (intFace[rdrLun]).protocol = protocol;
    
    // Get the USB device the intereface is part of
     ior = (*(intFace[rdrLun]).iface)->GetDevice((intFace[rdrLun]).iface,
                                                &USBDevice);
     if (ior)
     {
         LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Couldn't get the parent device  (0x%08X)", ior);
         (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
         return TrRv_ERR;
     }        
     
     // Get the plug-in for the device
     
     kr = IOCreatePlugInInterfaceForService(USBDevice, kIOUSBDeviceUserClientTypeID,
                                            kIOCFPlugInInterfaceID, 
                                            &ioPlugin, &score);
     IOObjectRelease(USBDevice);	/* done with the device object now */
     if (kr || !ioPlugin)
     {
         LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                     "unable to create a plugin for device (0x%08X)", kr);
         (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
         return TrRv_ERR;
     }
     
     // Get the device 
     res = (*ioPlugin)->QueryInterface(ioPlugin, 
                                       CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245),
                                       (LPVOID)&(intFace[rdrLun]).dev);
	IODestroyPlugInInterface(ioPlugin);
     if (res || !(intFace[rdrLun]).dev)
     {
         LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                     "Couldn't create a device  (0x%08X)", res);
         (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
         return TrRv_ERR;
     }
     

     // Open the device using Apple's KB example to resolve arbitration
     // issues with Classic
    for (sleepCount = 5; sleepCount > 0; sleepCount--)
    {
        kr = (*(intFace[rdrLun]).dev)->USBDeviceOpen(((intFace[rdrLun]).dev));
        if (kr == kIOReturnExclusiveAccess)
        {
            sleep(1);
        }
        else
        {
            if ( kr != kIOReturnSuccess)
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                            "unable to open device, not kIOReturnExclusiveAccess: 0x%08X", kr);
                (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
                (*(intFace[rdrLun]).dev)->Release(((intFace[rdrLun]).dev));
                return TrRv_ERR;
            }
            else
            {
                break;
            }
        }
    }
    if ( kr != kIOReturnSuccess)
    {
        // Some process is still using the device
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to open device, device busy");
        (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
        (*(intFace[rdrLun]).dev)->Release(((intFace[rdrLun]).dev));
        return TrRv_ERR;
    }

    (intFace[rdrLun]).usbAddr   = usbAddr;
     // Now the device is used but not set-up yet (pipes,...)
    (intFace[rdrLun]).used      = 1;
    
    
    
    // Get and store the device VendorID/ProductID
    (intFace[rdrLun]).vendorID = 0;
    ior = (*(intFace[rdrLun]).iface)->GetDeviceVendor(((intFace[rdrLun]).iface),
                                                (&((intFace[rdrLun]).vendorID)));
    if (ior)
    { 
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "unable to get device vendor Id");
    }                                        

    (intFace[rdrLun]).productID = 0;
    ior = (*(intFace[rdrLun]).iface)->GetDeviceProduct(((intFace[rdrLun]).iface),
                                                      (&((intFace[rdrLun]).productID)));
    if (ior)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "unable to get device product Id");
    }                                        
    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                "Driver captured device with vendor Id  %04X\n", 
                (intFace[rdrLun]).vendorID);
    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                "Driver captured device with product Id  %04X\n", 
                (intFace[rdrLun]).productID);
    
    // Read the USB strings to identify in the logs which reader 
    // was captured
    
    // Index of the manufacturer, product and serial number strings
    UInt8  manIdx, prodIdx, snIdx;
    kr = (*(intFace[rdrLun]).dev)->USBGetManufacturerStringIndex((intFace[rdrLun]).dev, 
                                                                 &manIdx);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                    "Could not get Manufacturer string");
    }       
    else
    {
        ReadUSBString((intFace[rdrLun]).dev, manIdx, "manufacturer");
    }
    kr = (*(intFace[rdrLun]).dev)->USBGetProductStringIndex((intFace[rdrLun]).dev, 
                                                                 &prodIdx);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                    "Could not get Product string");
    }       
    else
    {
        ReadUSBString((intFace[rdrLun]).dev, prodIdx, "product name");
    }

    kr = (*(intFace[rdrLun]).dev)->USBGetSerialNumberStringIndex((intFace[rdrLun]).dev, 
                                                                 &snIdx);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, 
                    "Could not get Serial Number string");
    }       
    else
    {
        ReadUSBString((intFace[rdrLun]).dev, snIdx, "serial number");
    }
    
    
    
    
    // Now release the interface as the call to SetConfiguration on
    // the device in SetupConnectionsUSB() will break it
    (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
    (intFace[rdrLun]).iface = 0;

    return TrRv_OK;  
}


TrRv GetConfigDescNumberUSB( DWORD lun, BYTE* pcconfigDescNb )
{
    DWORD		rdrLun;
    IOUSBDeviceInterface245 **dev;
    IOReturn err;

    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    
    // Check if a USB connection is set-up for this lun
    if ( ! (intFace[rdrLun]).used )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get class desc: usb not opened for lun %d", lun);
        return TrRv_ERR;
    }
    dev = (intFace[rdrLun]).dev;
    err = (*dev)->GetNumberOfConfigurations(dev, (UInt8 *) pcconfigDescNb);
    if (err || !(*pcconfigDescNb))
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to obtain the number of configurations. ret = %08x\n", 
                    err);
        return TrRv_ERR;
    }
    return TrRv_OK;  
}

TrRv GetVendorAndProductIDUSB( DWORD lun, DWORD *vendorID, DWORD *productID )
{
    DWORD		rdrLun;

    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    
    // Check if a USB connection is set-up for this lun
    if ( ! (intFace[rdrLun]).used )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get vendor or product ID: usb not opened for lun %d", lun);
        return TrRv_ERR;
    }
    *vendorID  = (intFace[rdrLun]).vendorID;
    *productID = (intFace[rdrLun]).productID;
    return TrRv_OK;
}


TrRv GetClassDescUSB( DWORD lun, BYTE configDescNb, BYTE bdescType,
                      BYTE *pcdesc, BYTE *pcdescLength)
{
    IOUSBDeviceInterface245 **dev;
    IOReturn err;
    DWORD		rdrLun;
    IOUSBConfigurationDescriptorPtr     confDesc;

    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    
    // Check if a USB connection is set-up for this lun
    if ( ! (intFace[rdrLun]).used )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get class desc: usb not opened for lun %d", lun);
        return TrRv_ERR;
    }
    dev = (intFace[rdrLun]).dev;
    UInt8 numConf;
    err = (*dev)->GetNumberOfConfigurations(dev, &numConf);
    if (err || !numConf)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to obtain the number of configurations. ret = %08x\n", err);
        return TrRv_ERR;
    }
    LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose, 
                "found %d configurations\n", numConf);
    if ( configDescNb >= numConf )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Requested configuration nb too large: %d\n", configDescNb);
        return TrRv_ERR;
    }
    
    err = (*dev)->GetConfigurationDescriptorPtr(dev, configDescNb, &confDesc);
    if (err)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get config descriptor for index %d\n", configDescNb);
        return TrRv_ERR;
    }
    char *ptr;
    ptr = (char*) confDesc;
    //+++  NEED TO DO THIS IN A NICER WAY (MIGHT BE FIXED IN LATER MOSX)
    //+ This is currently done to fix the endianness of the length
    //+ as the API returns the length in USB endianness
    unsigned char lsb, msb;
    lsb = *((char*)&(confDesc->wTotalLength));
    msb = *((char*)&(confDesc->wTotalLength)+1);
    UInt16 size, offset, offset_target_desc = 0;
    UInt8 found_target_desc = 0;
    
    size = (msb << 8) +lsb;
    // Move to beginning of descriptors
    
    //+ For some reason, sizeof(IOUSBConfigurationDescriptor)
    //+ Does not have the right length. It is probably 
    //+ not "packed" properly in the declaration.
    offset = 9; //sizeof(IOUSBConfigurationDescriptor);
    ptr += 9; //sizeof(IOUSBConfigurationDescriptor);

    // Scan all descriptors
    while ( offset < size )
    {
        //-LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Found new descriptor,");
        //-LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Length = %02Xh,", *(ptr));
        //-LogMessage( __FILE__,  __LINE__, LogLevelVerbose, " Type= %02X\n", *(ptr+1));
        if (  *(ptr+1) == bdescType )
        {
            offset_target_desc  = offset;
            found_target_desc = 1;
        }
        offset += *ptr;
        ptr += *ptr;
    }
    if (!found_target_desc)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, " unable to find target conf desc %02X\n", bdescType);
        return TrRv_ERR;
    }
    // Reset ptr to start of config desc
    ptr = (char*) confDesc;
    UInt8 targetDescLength = *(ptr+offset_target_desc);
    // Check if called to find out length or to return value
    if ( pcdesc == NULL )
    {
        *pcdescLength = targetDescLength;
        return TrRv_OK;
    }
    // Set length to minimal of buffer or real value
    if ( *pcdescLength > targetDescLength )
    {
        *pcdescLength = targetDescLength;
    }
    bcopy(ptr+offset_target_desc, pcdesc, *pcdescLength);
    return TrRv_OK;
}

//+++ NEED TO ADD and interruptPipe management
TrRv SetupConnectionsUSB( DWORD lun, BYTE ConfigDescNb, BYTE interruptPipe)
{
    kern_return_t                       kr;
    int                                 i = 0;
    IOUSBConfigurationDescriptorPtr     confDesc;
    UInt8                               intfNumEndpoints;
    UInt8                               direction, number, transferType, interval;
    UInt16                              maxPacketSize;    
    io_service_t                        USBIface = 0;
    io_iterator_t                       iter = 0;
    IOUSBFindInterfaceRequest           findInterface;
    IOCFPlugInInterface                 **iodevB = 0;
    HRESULT                             res;
    SInt32                              score;
    DWORD                               rdrLun;
    // Check if a USB connection is set-up for this lun
    rdrLun = lun >> 16;

    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    if ( ! (intFace[rdrLun]).used )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "unable to get set-up connections: usb not opened for lun %d", lun);
        return TrRv_ERR;
    }
    

    kr = (*(intFace[rdrLun]).dev)->GetConfigurationDescriptorPtr(((intFace[rdrLun]).dev),
                                                                 ConfigDescNb, &confDesc);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "ERR: unable to get the configuration: 0x%08X\n", kr);
        return TrRv_ERR;
    }

    (*(intFace[rdrLun]).dev)->ResetDevice(((intFace[rdrLun]).dev));

    
    // This call invalidates any interface currently opened on the device.
    // Hence the re-opening of the USB interface below (even though it was 
    // already opened in OpenUSB()
    kr = (*(intFace[rdrLun]).dev)->SetConfiguration(((intFace[rdrLun]).dev),
                                                    confDesc->bConfigurationValue);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "ERR: unable to set the configuration: 0x%08X\n", kr);
        return TrRv_ERR;
    }


 
    // Time to find the first CCID interface 

    findInterface.bInterfaceClass    = (intFace[rdrLun]).class;
    findInterface.bInterfaceSubClass = (intFace[rdrLun]).subClass;
    findInterface.bInterfaceProtocol = (intFace[rdrLun]).protocol;
    findInterface.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

    kr = (*(intFace[rdrLun]).dev)->CreateInterfaceIterator(((intFace[rdrLun]).dev),
                                                            &findInterface, &iter);
    if ( kr )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Can not create interface Iterator");
        return TrRv_ERR;
    }

    USBIface = IOIteratorNext(iter);
    //+++ We do not support a device with 2 CCID interfaces on it as 
    //++ pcscd does not support it either (Lun management would need to
    //++ be upgraded).
    IOObjectRelease(iter);
    iter = 0;
    
    if ( USBIface == 0 ) {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "No interface found");
        return TrRv_ERR;
    }

    score = 0;


    // Create the plugin for the interface service 
    kr  = IOCreatePlugInInterfaceForService(USBIface, kIOUSBInterfaceUserClientTypeID,
                                            kIOCFPlugInInterfaceID, &iodevB, &score);
    if ( kr || !iodevB )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "No interface found");
        IOObjectRelease(USBIface);
        return TrRv_ERR;        
    }

    // Now get the real interface
     res = (*iodevB)->QueryInterface(iodevB, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245),
                                   (LPVOID)&(intFace[rdrLun]).iface);
	IODestroyPlugInInterface(iodevB);	// done with this
    IOObjectRelease(USBIface);
    if ( res || !(intFace[rdrLun]).iface)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Could not query interface");
        return TrRv_ERR;
    }


    // Open the interface to open all the pipes 
    kr = (*(intFace[rdrLun]).iface)->USBInterfaceOpen((intFace[rdrLun]).iface);
    if ( kr )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "unable to open device: 0x%08X", kr);
        return TrRv_ERR;
    }

    
    
    // Get nb of end points
    kr = (*(intFace[rdrLun]).iface)->GetNumEndpoints((intFace[rdrLun]).iface, &intfNumEndpoints);
    if (kr != kIOReturnSuccess )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "unable to get number of end points: 0x%08X", kr);
        return TrRv_ERR;
    }
    // pipes are one based, since zero is the default control pipe
    for (i=1; i <= intfNumEndpoints; i++)
    {
        kr = (*(intFace[rdrLun]).iface)->GetPipeProperties((intFace[rdrLun]).iface, i,
                                                           &direction, &number,
                                                           &transferType, &maxPacketSize,
                                                           &interval);
        if (kr != kIOReturnSuccess )
        {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                        "unable to get pipe properties: 0x%08X", kr);
            return TrRv_ERR;
        }
        if (transferType != kUSBBulk)
        {
            continue;
        }
        if ((direction == kUSBIn) && !((intFace[rdrLun]).inPipeRef))
        {
            (intFace[rdrLun]).inPipeRef = i;
        }
        if ((direction == kUSBOut) && !((intFace[rdrLun]).outPipeRef))
        {
            (intFace[rdrLun]).outPipeRef = i;
        }
        //+++ Need to add optional management of interrupt pipe

    }


    if ( !((intFace[rdrLun]).outPipeRef) )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get outPipe: 0x%08X", kr);
        CloseUSB(lun);
        return TrRv_ERR;
    }

    if (!( (intFace[rdrLun]).inPipeRef))
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "unable to get inPipe: 0x%08X", kr);
        CloseUSB(lun);
        return TrRv_ERR;
    }
    LogMessage( __FILE__,  __LINE__, LogLevelImportant,
                "New reader fully set-up at USB address: %08X\n", 
                (intFace[rdrLun]).usbAddr);
    (intFace[rdrLun]).ready = 1;
    return TrRv_OK;
}



TrRv WriteUSB( DWORD lun, DWORD length, unsigned char *buffer )
{
    IOReturn		iorv;
    DWORD		rdrLun;
     
    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    if ( ! (intFace[rdrLun]).ready )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "unable to write to USB: set-up not completed for lun %d", lun);
        return TrRv_ERR;
    }
    
    LogHexBuffer(__FILE__,  __LINE__, LogLevelVeryVerbose, buffer, length, 
                 "Attempt to write: ");
        
    /* Make sure the pipe is OK */
    iorv = (*(intFace[rdrLun]).iface)->GetPipeStatus( (intFace[rdrLun]).iface, 
                                                            (intFace[rdrLun]).outPipeRef );
    if ( iorv != kIOReturnSuccess )
    {
        return TrRv_ERR;
    }

    /* Write the data */
    iorv = (*(intFace[rdrLun]).iface)->WritePipe((intFace[rdrLun]).iface,
                                                  (intFace[rdrLun]).outPipeRef,
                                                  buffer, length);
    
    if ( iorv != kIOReturnSuccess )
    {
        return TrRv_ERR;
    }
            
    return TrRv_OK;
}

TrRv ReadUSB( DWORD lun, DWORD *length, unsigned char *buffer )
{
    IOReturn	iorv;
    UInt32		recvLen;
    UInt32      noDataTO, completeTO;
    DWORD		rdrLun;

    
    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "OpenUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    if ( ! (intFace[rdrLun]).ready )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "unable to write to USB: set-up not completed for lun %d", lun);
        return TrRv_ERR;
    }
    
    LogMessage( __FILE__,  __LINE__, LogLevelVeryVerbose,
                "Attempt to read %ld bytes", *length);
    
    /* Make sure the pipe is OK */
    iorv = (*(intFace[rdrLun]).iface)->GetPipeStatus( (intFace[rdrLun]).iface,
                                                       (intFace[rdrLun]).inPipeRef
                                                       );
    if ( iorv != kIOReturnSuccess )
    {
        return TrRv_ERR;
    }

    recvLen = *length;
    completeTO = ReadTimeOut;
    noDataTO   = ReadTimeOut;

    iorv = (*(intFace[rdrLun]).iface)->ReadPipeTO( (intFace[rdrLun]).iface,
                                                    (intFace[rdrLun]).inPipeRef,
                                                    buffer, &recvLen, noDataTO, completeTO);
    if ( iorv != 0 )
    {
        (*(intFace[rdrLun]).dev)->ResetDevice(((intFace[rdrLun]).dev));
        return TrRv_ERR;
    }
    
    LogHexBuffer(__FILE__,  __LINE__, LogLevelVeryVerbose, buffer, recvLen, "received: ");
    
    *length = recvLen;
    return TrRv_OK;
}

TrRv CloseUSB( DWORD lun )
{
    IOReturn iorv;
    DWORD rdrLun;
    
    rdrLun = lun >> 16;
    if ( rdrLun >= USBMAX_READERS )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "CloseUSB error: lun too large: %08X", lun);
        return TrRv_ERR;
    }
    
    // Reset struct
    (intFace[rdrLun]).usbAddr = 0;
    (intFace[rdrLun]).outPipeRef = 0;
    (intFace[rdrLun]).inPipeRef = 0;
    (intFace[rdrLun]).used = 0;
    (intFace[rdrLun]).ready = 0;
    (intFace[rdrLun]).class = 0;
    (intFace[rdrLun]).subClass = 0;            
    

    /* Close the interface */
    // Check if it was allocated
    if ( (intFace[rdrLun]).iface )
    {
        iorv = (*(intFace[rdrLun]).iface)->USBInterfaceClose( (intFace[rdrLun]).iface );
        if (iorv)
        {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                        "ERR: Couldn't close interface (%08x)\n", (int)iorv);
        }

        /* Release the interface */
        (*(intFace[rdrLun]).iface)->Release((intFace[rdrLun]).iface);
        (intFace[rdrLun]).iface = 0;
    }
    if ( (intFace[rdrLun]).dev )
    {
        iorv = (*(intFace[rdrLun]).dev)->USBDeviceClose((intFace[rdrLun]).dev);
        if (iorv) {
            LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                        "ERR: Couldn't close device (%08x)\n", (int)iorv);
        }

        (*(intFace[rdrLun]).dev)->Release((intFace[rdrLun]).dev);
        if ( iorv != kIOReturnSuccess ) {
            return TrRv_ERR;
        }
        (intFace[rdrLun]).dev = 0;
    }
    return TrRv_OK;
}




void ReadUSBString(IOUSBDeviceInterface245 **dev, UInt8 bIndex,
                   const char* pcHeader)
{
    IOUSBDevRequest stDevRequest;
    unsigned char pcArray[512];    
    kern_return_t		kr;
    
    // Generate the USB request manually
    stDevRequest.bmRequestType = USBmakebmRequestType( kUSBIn, kUSBStandard, 
                                                       kUSBDevice );
    stDevRequest.bRequest      = kUSBRqGetDescriptor;
    stDevRequest.wValue        = ( kUSBStringDesc << 8 ) | bIndex;
    // Select language
    stDevRequest.wIndex        = LANGUAGE_ID;
    stDevRequest.wLength       = sizeof(pcArray);
    stDevRequest.pData         = (void *) pcArray;
    bzero(pcArray, sizeof(pcArray));
    kr = (*dev)->DeviceRequest(dev, &stDevRequest);
    if (kr)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "DeviceRequest for USB string failed %08X", kr);
        return;
    }
    
    // Check that we got what we wanted
    if ( pcArray[1] != STRING_REQUEST )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "DeviceRequest for USB string did not return expected data");
        return;
    }
    
    CFStringRef cfstr;
    UInt8 tmp = pcArray[0];
    // Add Unicode BOM to the returned string to allow CFString to 
    // work. array[0:1] stores the length of the Unicode string
    // and a type byte
    pcArray[0] = 0xFF;
    pcArray[1] = 0xFE;
    
    cfstr = CFStringCreateWithBytes (
                                     kCFAllocatorDefault,
                                     pcArray,
                                     // Length in BYTES
                                     tmp,
                                     kCFStringEncodingUnicode,
                                     1
                                     );
    
    
    // Turn the CFString in a standard C string    
    if ( !CFStringGetCString (
                              cfstr,
                              (char *)pcArray,
                              sizeof(pcArray)-1,
                              kCFStringEncodingASCII
                              )
         )
    {
       LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Conversion of USB string to ASCII failed, printing raw data");
       LogHexBuffer(__FILE__,  __LINE__, LogLevelCritical, pcArray+2, tmp, 
                    "Raw data: "); 
       CFRelease(cfstr);
       return;
        
    }
    // Manually ensure string is terminated
    pcArray[sizeof(pcArray)-1] = '\0';
    CFRelease(cfstr);
    LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                "Captured device %s is %s", pcHeader, pcArray);
    
    return;
}
