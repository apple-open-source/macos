/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <sys/errno.h> 
#include <sys/fcntl.h>
#include <sys/stat.h>   
    
#include <mach/mach.h>
#include <mach/mach_error.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFPlugin.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#define mach_test(expr, msg) do {				\
    kern_return_t err = (expr);					\
    if (KERN_SUCCESS != err) {					\
        fprintf(stderr, "%s: %s - %s(%x,%d)\n", cmdName, 	\
                msg, mach_error_string(err),			\
                err, err & 0xffffff);				\
        fflush(stderr);						\
        *((char *) 0) = 0;					\
    }								\
} while(0)

#define plain_test(expr, msg) do {				\
    if (expr) {							\
        fprintf(stderr, "%s: %s\n", cmdName,  msg);		\
        fflush(stderr);						\
        *((char *) 0) = 0;					\
    }								\
} while(0)

static const char *cmdName;
static Boolean runAsync = 0;

static volatile void usage(void)
{
    fprintf(stderr, "Usage: %s [-a]", cmdName);
    fprintf(stderr, " where -a indicates async run");
    exit(EX_USAGE);
}

static void init(int argc, const char *argv[])
{
    char c;

    // Get the command name and strip the leading dirpath
    if (cmdName = strrchr(argv[0], '/'))
        cmdName++;
    else
        cmdName = argv[0];

    while ((c = getopt(argc, argv, "a")) != -1) {
        switch(c) {
        case 'a': runAsync = 1; break;
        case '?':
        default:
            usage();
        }
    }
}


static void 
showDeviceInfo(IOUSBDeviceInterface **dev)
{
    UInt8	devClass;
    UInt8	devSubClass;
    UInt8	devProtocol;
    UInt16	devVendor;
    UInt16	devProduct;
    UInt16	devRelNum;
    
    IOReturn kr;

    kr = (*dev)->GetDeviceClass(dev, &devClass);
    mach_test(kr, "Couldn't get class");
    kr = (*dev)->GetDeviceSubClass(dev, &devSubClass);
    mach_test(kr, "Couldn't get subclass");
    kr = (*dev)->GetDeviceProtocol(dev, &devProtocol);
    mach_test(kr, "Couldn't get protocol");
    kr = (*dev)->GetDeviceVendor(dev, &devVendor);
    mach_test(kr, "Couldn't get vendor");
    kr = (*dev)->GetDeviceProduct(dev, &devProduct);
    mach_test(kr, "Couldn't get product");
    kr = (*dev)->GetDeviceReleaseNumber(dev, &devRelNum);
    mach_test(kr, "Couldn't get relnum");
    printf("----------------------------\n");
    printf("USB device\n");
    printf("----------------------------\n");
    printf("\tclass: %d\n", devClass);
    printf("\tsubclass: %d\n", devSubClass);
    printf("\tprotocol: %d\n", devProtocol);
    printf("\tVendor: %04x\n", devVendor);
    printf("\tProduct: %04x\n", devProduct);
    printf("\tRelease: %04x\n", devRelNum);
}

static void dosomethingtodevice(io_service_t USBDevice);

static void 
showInterfaceInfo(IOUSBInterfaceInterface **intf)
{
    UInt8		intfClass;
    UInt8		intfSubClass;
    UInt8		intfProtocol;
    UInt16		devVendor;
    UInt16		devProduct;
    UInt16		devRelNum;
    UInt8		configValue;
    UInt8		intfNumber;
    io_service_t	USBdevice = 0;
    
    IOReturn kr;

    kr = (*intf)->GetInterfaceClass(intf, &intfClass);
    mach_test(kr, "Couldn't get class");
    kr = (*intf)->GetInterfaceSubClass(intf, &intfSubClass);
    mach_test(kr, "Couldn't get subclass");
    kr = (*intf)->GetInterfaceProtocol(intf, &intfProtocol);
    mach_test(kr, "Couldn't get protocol");
    kr = (*intf)->GetDeviceVendor(intf, &devVendor);
    mach_test(kr, "Couldn't get vendor");
    kr = (*intf)->GetDeviceProduct(intf, &devProduct);
    mach_test(kr, "Couldn't get product");
    kr = (*intf)->GetDeviceReleaseNumber(intf, &devRelNum);
    mach_test(kr, "Couldn't get relnum");
    kr = (*intf)->GetConfigurationValue(intf, &configValue);
    mach_test(kr, "Couldn't get configvalue");
    kr = (*intf)->GetInterfaceNumber(intf, &intfNumber);
    mach_test(kr, "Couldn't get interface number");
    kr = (*intf)->GetDevice(intf, &USBdevice);
    mach_test(kr, "Couldn't get device");
    printf("----------------------------\n");
    printf("USB interface\n");
    printf("----------------------------\n");
    printf("\tclass: %d\n", intfClass);
    printf("\tsubclass: %d\n", intfSubClass);
    printf("\tprotocol: %d\n", intfProtocol);
    printf("\tVendor: %04x\n", devVendor);
    printf("\tProduct: %04x\n", devProduct);
    printf("\tRelease: %04x\n", devRelNum);
    printf("\tConfigValue: %d\n", configValue);
    printf("\tInterfaceNumber: %d\n", intfNumber);
    dosomethingtodevice(USBdevice);
}


#if 0
static void printinquirydata(USBInquiry *d, UInt32 s)
{
    char vendorName[sizeof(d->vendorName)+1];
    char productName[sizeof(d->productName)+1];
    char productRevision[sizeof(d->productRevision)+1];
    char vendorSpecific[sizeof(d->vendorSpecific)+1];
    int i, size;

    vendorName[sizeof(d->vendorName)] = '\0';
    strncpy(vendorName, d->vendorName, sizeof(d->vendorName));

    productName[sizeof(d->productName)] = '\0';
    strncpy(productName, d->productName, sizeof(d->productName));

    productRevision[sizeof(d->productRevision)] = '\0';
    strncpy(productRevision, d->productRevision, sizeof(d->productRevision));

    vendorSpecific[sizeof(d->vendorSpecific)] = '\0';
    strncpy(vendorSpecific, d->vendorSpecific, sizeof(d->vendorSpecific));

    printf("%s: Got inquiry results size = %ld\n", cmdName, s);

    printf("  devType = 0x%x\n", d->devType);
    printf("  devTypeMod = 0x%x\n", d->devTypeMod);
    printf("  version = 0x%x\n", d->version);
    printf("  format = 0x%x\n", d->format);
    printf("  length = 0x%x\n", d->length);
    printf("  reserved5 = 0x%x\n", d->reserved5);
    printf("  reserved6 = 0x%x\n", d->reserved6);
    printf("  flags = 0x%x\n", d->flags);
    printf("  vendorName = \"%s\"\n", vendorName);
    printf("  productName = \"%s\"\n", productName);
    printf("  productRevision = \"%s\"\n", productRevision);
    printf("  vendorSpecific = \"%s\"\n", vendorSpecific);

    size = s - ((char *) &d->moreReserved - (char *) d);
    if (size > 0) {
        printf("  moreReserved(%d) = ", size);
        for (i = 0; i < size; i++) {
            if ( !(i & 0xf) )
                printf("\n");
            printf("%02x ", ((UInt8 *) d->moreReserved)[i]);
        }
        printf("\n");
    }
}


// assumes device is open
static IOCDBCommandInterface **
allocCommand(IOUSBDeviceInterface **dev)
{
    HRESULT res;
    IOCDBCommandInterface **cmd;

    printf("%s: opened device\n", cmdName);
    res = (*dev)->QueryInterface(dev,
        CFUUIDGetUUIDBytes(kIOCDBCommandInterfaceID), (LPVOID) &cmd);
    plain_test(res != S_OK, "Couldn't create a CDB Command");
    
    return cmd;
}

// assumes device is open
static void executeSyncInquiry(IOCDBCommandInterface **cmd)
{
    UInt8 data[255];
    IOVirtualRange range[1];
    CDBInfo cdb;
    USBResults results;
    UInt32 seqNumber;
    IOReturn kr;

    bzero(data, sizeof(data));

    range[0].address = (IOVirtualAddress) data;
    range[0].length  = sizeof(data);

    bzero(&cdb, sizeof(cdb));
    cdb.cdbLength = 6;
    cdb.cdb[0] = kUSBCmdInquiry;
    cdb.cdb[4] = sizeof(data);

    kr = (*cmd)->setAndExecuteCommand(cmd,
                                      &cdb,
                                      sizeof(data),
                                      range,
                                      sizeof(range)/sizeof(range[0]),
                        /* isWrite */ 0,
                      /* timeoutMS */ 0,
                         /* target */ 0,
                       /* callback */ 0,
                         /* refcon */ 0,
                                      &seqNumber);
    if (kr == kIOReturnUnderrun)
        printf("%s: Command Underrun\n", cmdName);
    else
        mach_test(kr, "Couldn't execute a CDB Command");

    kr = (*cmd)->getResults(cmd, &results);
    if (kr == kIOReturnUnderrun)
        printf("%s: getResults Underrun\n", cmdName);
    else
        mach_test(kr, "Couldn't get results of a command");

    printinquirydata((USBInquiry *) data, results.bytesTransferred);
}
#endif

static void 
dosomethingtodevice(io_service_t USBDevice)
{
    io_name_t 			className;
    IOCFPlugInInterface 	**iodev;
    IOUSBDeviceInterface 	**dev;
    HRESULT 			res;
    kern_return_t 		kr;
    SInt32 			score;

    printf("Doing something to device: %d (%08x)\n", USBDevice, USBDevice);
    mach_test(IOObjectGetClass(USBDevice, className),
            "Failed to get class name");

    printf("%s: found device type %s\n", cmdName, className);

    kr = IOCreatePlugInInterfaceForService(USBDevice,
                                                kIOUSBDeviceUserClientTypeID, 
                                                kIOCFPlugInInterfaceID,
                                                &iodev,
                                                &score);
    //mach_test(kr, "Couldn't create a plug in");
    if (kr)
        printf("--unable to create plugin\n");
    else
        printf("!!PLUGIN SUCCESS\n");
    res = (*iodev)->QueryInterface(iodev,
        CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID) &dev);
    plain_test(res != S_OK, "Couldn't create USB Device interface");
    (*iodev)->Release(iodev);

    showDeviceInfo(dev);
#if 0

    kr = (*dev)->open(dev);
    if (kIOReturnSuccess != kr)
        printf("%s: failed to open 0x%08x\n", cmdName, kr);
    else {
        IOCDBCommandInterface **cmd = allocCommand(dev);
        if (cmd) {
            executeSyncInquiry(cmd);
            (*cmd)->Release(cmd);
        }
        (*dev)->close(dev);
    }

    (*dev)->Release(dev);
#endif

    IOObjectRelease(USBDevice);
}


static void
dosomethingtointerface(io_service_t USBInterface)
{
    io_name_t 			className;
    IOCFPlugInInterface 	**iodev;
    IOUSBInterfaceInterface 	**intf;
    HRESULT 			res;
    kern_return_t 		kr;
    SInt32 			score;

    printf("Doing something to interface: %d (%08x)\n", USBInterface, USBInterface);
    mach_test(IOObjectGetClass(USBInterface, className),
            "Failed to get class name");

    printf("%s: found device type %s\n", cmdName, className);

    kr = IOCreatePlugInInterfaceForService(USBInterface,
                                                kIOUSBInterfaceUserClientTypeID, 
                                                kIOCFPlugInInterfaceID,
                                                &iodev,
                                                &score);
    //mach_test(kr, "Couldn't create a plug in");
    if (kr)
    {
        printf("--unable to create plugin\n");
        return;
    }
    else
        printf("!!PLUGIN SUCCESS\n");
    res = (*iodev)->QueryInterface(iodev,
        CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID) &intf);
    plain_test(res != S_OK, "Couldn't create USB Interface interface");
    (*iodev)->Release(iodev);

    showInterfaceInfo(intf);
#if 0

    kr = (*dev)->open(dev);
    if (kIOReturnSuccess != kr)
        printf("%s: failed to open 0x%08x\n", cmdName, kr);
    else {
        IOCDBCommandInterface **cmd = allocCommand(dev);
        if (cmd) {
            executeSyncInquiry(cmd);
            (*cmd)->Release(cmd);
        }
        (*dev)->close(dev);
    }

    (*dev)->Release(dev);
#endif

    IOObjectRelease(USBInterface);
}


static void 
findUSBDevices(void)
{
    // this function will interate through all IOUSBDevice nubs and spit out information about them
    mach_port_t 	masterPort = 0;
    CFDictionaryRef 	USBMatch = 0;
    io_iterator_t 	iter = 0;
    io_service_t 	USBDevice = 0;

    mach_test(IOMasterPort(bootstrap_port, &masterPort),
              "Couldn't create a master IOKit Port");

    USBMatch = IOServiceMatching(kIOUSBDeviceClassName);
    plain_test(!USBMatch, "Can't create a USB matching dictionary");

    mach_test(IOServiceGetMatchingServices(masterPort, USBMatch, &iter),
              "Can't create a USB Service iterator");
    USBMatch = 0;	// Finish hand off of USBMatch dictionary

    while ( (USBDevice = IOIteratorNext(iter)) )
        dosomethingtodevice(USBDevice);

    if (iter) 
    {
        IOObjectRelease(iter);
        iter = 0;
    }

    if (USBMatch) 
    {
        CFRelease(USBMatch);
        USBMatch = 0;
    }

    if (masterPort) 
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        masterPort = 0;
    }
}


static void findUSBInterfaces(void)
{
    mach_port_t 	masterPort = 0;
    CFDictionaryRef 	USBMatch = 0;
    io_iterator_t 	iter = 0;
    io_service_t 	USBInterface = 0;

    // this function will iterate through all IOUSBInterface nubs and spit out information
    // about them
    mach_test(IOMasterPort(bootstrap_port, &masterPort),
              "Couldn't create a master IOKit Port");

    USBMatch = IOServiceMatching(kIOUSBInterfaceClassName);
    plain_test(!USBMatch, "Can't create a USB matching dictionary");

    mach_test(IOServiceGetMatchingServices(masterPort, USBMatch, &iter),
              "Can't create a USB Service iterator");
    USBMatch = 0;	// Finish hand off of USBMatch dictionary

    while ( (USBInterface = IOIteratorNext(iter)) )
        dosomethingtointerface(USBInterface);

    if (iter) 
    {
        IOObjectRelease(iter);
        iter = 0;
    }

    if (USBMatch) 
    {
        CFRelease(USBMatch);
        USBMatch = 0;
    }

    if (masterPort) 
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        masterPort = 0;
    }
}

static unsigned char desc[1024];
CFRunLoopSourceRef	cfSource;

void
MyCallBackFunction(IOUSBConfigurationDescriptorPtr desc, IOReturn result, void *arg0)
{
    printf("MyCallbackfunction: %d, %d, %d\n", (int)desc, (int)result, (int)arg0);
    CFRunLoopStop(CFRunLoopGetCurrent());
}


static void 
findUnconfiguredDevices(void)
{
    // this function will interate through all IOUSBDevice nubs and spit out information about them
    mach_port_t 	masterPort = 0;
    CFDictionaryRef 	USBMatch = 0;
    io_iterator_t 	iter = 0;
    io_service_t 	USBDevice = 0;
    kern_return_t	kr;

    // first create a master_port for my task
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return;
    }

    USBMatch = IOServiceMatching(kIOUSBDeviceClassName);
    if (!USBMatch)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return;
    }
    
    // create an iterator over all matching IOService nubs
    kr = IOServiceGetMatchingServices(masterPort, USBMatch, &iter);
    if (kr)
    {
        printf("Can't create a USB Service iterator(%08x)\n", kr);
        CFRelease(USBMatch);
        mach_port_deallocate(mach_task_self(), masterPort);
        return;
    }

    while ( (USBDevice = IOIteratorNext(iter)) )
    {
        IOCFPlugInInterface 			**iodev;
        IOUSBDeviceInterface 			**dev;
        HRESULT 				res;
        SInt32 					score;
        UInt8					numConf;
        UInt8					curConf = 0;
        UInt8					devClass = 0, devSubClass = 0;
        int					i;
        IOUSBConfigurationDescriptorPtr		confDesc;
        
        kr = IOCreatePlugInInterfaceForService(USBDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
        IOObjectRelease(USBDevice);				// done with the device object now that I have the plugin
        if (kr || !iodev)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }
            
        // i have the device plugin. I need the device interface
        res = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID)&dev);
        (*iodev)->Release(iodev);				// done with this
        if (res || !dev)
        {
            printf("couldn't create a device interface (%08x)\n", (int)res);
            continue;
        }
        // technically should check these kr values
        kr = (*dev)->GetConfiguration(dev, &curConf);
        kr = (*dev)->GetDeviceClass(dev, &devClass);
        kr = (*dev)->GetDeviceSubClass(dev, &devSubClass);
        if (curConf != 0)
        {
            printf("found configured device (class = %d, subclass = %d)\n", devClass, devSubClass);
            (*dev)->Release(dev);
            continue;
        }
        printf("found UNCONFIGURED device (class = %d, subclass = %d)\n", devClass, devSubClass);
        // need to open the device in order to change its state
        kr = (*dev)->USBDeviceOpen(dev);
        if (kr)
        {
            printf("unable to open device: %08x\n", kr);
            continue;
        }
            
        kr = (*dev)->GetNumberOfConfigurations(dev, &numConf);
        printf("\tfound %d configurations\n", numConf);
        for (i=0; i<numConf; i++)
        {
            kr = (*dev)->GetConfigurationDescriptorPtr(dev, i, &confDesc);
            if (kr)
            {
                printf("\tunable to get config descriptor for index %d\n", i);
                continue;
            }
            printf("\t--------------------------------\n");
            printf("\tConfig Descriptor index #%d\n", i);
            printf("\t--------------------------------\n");
            printf("\tTotal Length: %d\n", USBToHostWord(confDesc->wTotalLength));
            printf("\tConfig Value: %d\n", confDesc->bConfigurationValue);
            printf("\t--------------------------------\n");
            
            if (i==0)
            {
                printf("\tsetting the config to %d\n", confDesc->bConfigurationValue);
                kr = (*dev)->SetConfiguration(dev, confDesc->bConfigurationValue);
                kr = (*dev)->GetConfiguration(dev, &curConf);
                if (curConf != confDesc->bConfigurationValue)
                    printf("\t\tSET CONFIG FAILED!\n");
            }
        }
        printf("Trying to do a device request\n");
        {
            IOUSBDevRequest	request;
            
            // create a run loop to catch the async notifications
            kr = (*dev)->CreateDeviceAsyncEventSource(dev, &cfSource);
            CFRunLoopAddSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
            
            desc[0]='H'; desc[1]='i';
            
            request.bmRequestType = (kUSBIn << kUSBRqDirnShift)
                                    | (kUSBStandard << kUSBRqTypeShift)
                                    | kUSBDevice;
            request.bRequest = kUSBRqGetDescriptor;
            request.wValue = (kUSBConfDesc << 8) + 0;
            request.wIndex = 0;
            request.wLength = USBToHostWord(confDesc->wTotalLength);
            request.pData = desc;
            kr = (*dev)->DeviceRequestAsync(dev, &request, (IOAsyncCallback1)MyCallBackFunction, (void*)desc);
            if (kr)
                printf("device request failed\n");
            else
            {
                printf("about to call RunLoopRun\n");
                CFRunLoopRun();
                printf("RunLoopRun returned\n");
            }
            CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
        }
        kr = (*dev)->USBDeviceClose(dev);
        kr = (*dev)->Release(dev);
    }

    if (iter) 
    {
        IOObjectRelease(iter);
        iter = 0;
    }

    if (USBMatch) 
    {
        CFRelease(USBMatch);
        USBMatch = 0;
    }

    if (masterPort) 
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        masterPort = 0;
    }
}


static void 
findVendorSpecificInterfaces(void)
{
    // this function will interate through all IOUSBDevice nubs and spit out information about them
    mach_port_t 	masterPort = 0;
    CFDictionaryRef 	USBMatch = 0;
    io_iterator_t 	iter = 0;
    io_service_t 	USBInterface = 0;
    kern_return_t	kr;

    // first create a master_port for my task
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return;
    }

    USBMatch = IOServiceMatching(kIOUSBInterfaceClassName);
    if (!USBMatch)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return;
    }
    
    // create an iterator over all matching IOService nubs
    kr = IOServiceGetMatchingServices(masterPort, USBMatch, &iter);
    if (kr)
    {
        printf("Can't create a USB Service iterator(%08x)\n", kr);
        CFRelease(USBMatch);
        mach_port_deallocate(mach_task_self(), masterPort);
        return;
    }

    while ( (USBInterface = IOIteratorNext(iter)) )
    {
        IOCFPlugInInterface 			**iodev=NULL;
        IOUSBInterfaceInterface 		**intf=NULL;
        HRESULT 				res;
        SInt32 					score;
        int					i;
        UInt16					vendor;
        UInt16					product;
        UInt8					numPipes;
        
        kr = IOCreatePlugInInterfaceForService(USBInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
        IOObjectRelease(USBInterface);				// done with the device object now that I have the plugin
        if (kr || !iodev)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }
            
        // i have the device plugin. I need the device interface
        res = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID)&intf);
        (*iodev)->Release(iodev);				// done with this
        if (res || !intf)
        {
            printf("couldn't create a device interface (%08x)\n", (int)res);
            continue;
        }
        // technically should check these kr values
        kr = (*intf)->GetDeviceVendor(intf, &vendor);
        kr = (*intf)->GetDeviceProduct(intf, &product);
        if ((vendor != 1215) || (product != 275))
        {
            printf("found interface i didn't want (vendor = %d, product = %d)\n", vendor, product);
            (*intf)->Release(intf);
            continue;
        }
        printf("found my interface (vendor = %d, product = %d)\n", vendor, product);
        // need to open the interface in order to change its state
        kr = (*intf)->USBInterfaceOpen(intf);
        if (kr)
        {
            printf("unable to open interface: %08x\n", kr);
            continue;
        }
            
        kr = (*intf)->GetNumEndpoints(intf, &numPipes);
        printf("\tfound %d pipes\n", numPipes);
        for (i=1; i<=numPipes; i++)
        {
            UInt8	number, direction, transferType, interval;
            UInt16	mps;
            kr = (*intf)->GetPipeProperties(intf, i, &direction, &number, &transferType, &mps, &interval);
            printf("pipe %d: dir=%d, num=%d, tt=%d, mps=%d, interval=%d\n", i, direction, number, transferType, mps, interval);
        }
        kr = (*intf)->USBInterfaceClose(intf);
        kr = (*intf)->Release(intf);
    }

    if (iter) 
    {
        IOObjectRelease(iter);
        iter = 0;
    }

    if (USBMatch) 
    {
        CFRelease(USBMatch);
        USBMatch = 0;
    }

    if (masterPort) 
    {
        mach_port_deallocate(mach_task_self(), masterPort);
        masterPort = 0;
    }
}


__private_extern__ int 
main(int argc, const char *argv[])
{
    init(argc, argv);

    findUSBDevices();
    // findUSBInterfaces();
    //findUnconfiguredDevices();
    //findVendorSpecificInterfaces();
    exit(EX_OK);
}
