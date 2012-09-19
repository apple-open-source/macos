/*
 * Copyright © 2003-2012 Apple Inc. All rights reserved.
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
/*
* © Copyright 2001 Apple Inc.  All rights reserved.
*
* IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in 
* consideration of your agreement to the following terms, and your use, installation, 
* modification or redistribution of this Apple software constitutes acceptance of these
* terms.  If you do not agree with these terms, please do not use, install, modify or 
* redistribute this Apple software.
*
* In consideration of your agreement to abide by the following terms, and subject to these 
* terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
* original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
* the Apple Software, with or without modifications, in source and/or binary forms; provided 
* that if you redistribute the Apple Software in its entirety and without modifications, you 
* must retain this notice and the following text and disclaimers in all such redistributions 
* of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
* Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
* without specific prior written permission from Apple. Except as expressly stated in this 
* notice, no other rights or licenses, express or implied, are granted by Apple herein, 
* including but not limited to any patent rights that may be infringed by your derivative 
* works or by other works in which the Apple Software may be incorporated.
* 
* The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
* EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
* INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
* SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
*
* IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
* REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
* WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
* OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


    // This example works with a Cypress (previously Anchor) EZ-USB device.
    // It downloads firmware into the device, then runs a driver for the new device.
    // 
    // Usage:  ezload VID PID [PID2|VID2 PID2]
    // 
    // VID is a vendor ID for a USB device, PID is the product ID.
    // 
    // This will look for a device with the VID/PID combination to specified,
    // download the firmware, then attempt to load a driver.
    // 
    // The firmware is loaded from a file named with the hex value of VIDPID.HEX
    // for example, a device with VID 1452 (0x5AC) and PID 3000 (0xBB8) will try 
    // to load firmware from a file 05AC0BB8.HEX. This file can be in the current 
    // directory or in a directory on the PATH environment variable. If an 
    // environment vaiableHEXPATH is defined, this will instead of the PATH variable.
    // The HEX file is in Intel HEX format and is the sort of thing you'll get
    // in a Windows oriented developer kit for your device.
    // 
    // Once the firmware is loaded this will look for a device with the downloaded
    // VID/PID combination. If just VID and PID are specified, the new device expected 
    // is VID/PID+1, if PID2 is specified VID/PID2 is expected, if VID2 and PID2 are
    // specified VID2/PID2 is expected.
    // 
    // Once the new device is attached this attempts to run a driver for it, this
    // is an executable in a directory on the PATH, named similarly to the hex file
    // It is executed with the new VID and PID as arguments.
    // e.g. if VID=1452(0x5AC) and PID2=3001(0xBB9) this will attempt to execute:
    // 05AC0BB9 1452 3001
    
    // This example is based heavily on the USBNotification Example.


#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <mach/mach.h>

#include "hex2c.h"

#define	k8051_USBCS		0x7f92
#define NARRATEIO 0

// globals
static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gRawAddedIter;
static io_iterator_t		gRawRemovedIter;
static io_iterator_t		gNewDeviceAddedIter;
static io_iterator_t		gNewDeviceRemovedIter;

IOReturn ConfigureAnchorDevice(IOUSBDeviceInterface245 **dev)
{
    UInt8				numConf;
    IOReturn				kr;
    IOUSBConfigurationDescriptorPtr	confDesc;
    
    kr = (*dev)->GetNumberOfConfigurations(dev, &numConf);
    if (!numConf)
        return -1;
    
    // get the configuration descriptor for index 0
    kr = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &confDesc);
    if (kr)
    {
        printf("\tunable to get config descriptor for index %d (err = %08x)\n", 0, kr);
        return -1;
    }
    kr = (*dev)->SetConfiguration(dev, confDesc->bConfigurationValue);
    if (kr)
    {
        printf("\tunable to set configuration to value %d (err=%08x)\n", 0, kr);
        return -1;
    }
    
    return kIOReturnSuccess;
}

IOReturn AnchorWrite(IOUSBDeviceInterface245 **dev, UInt16 anchorAddress, UInt16 count, UInt8 writeBuffer[])
{
    IOUSBDevRequest 		request;
    
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    request.bRequest = 0xa0;
    request.wValue = anchorAddress;
    request.wIndex = 0;
    request.wLength = count;
    request.pData = writeBuffer;

    return (*dev)->DeviceRequest(dev, &request);
}


void runDeviceDriver(UInt16 vendor, UInt16 product)
{
char name[]="12341234",
    vendorStr[]="65535",
    prodStr[]="65535";
pid_t newPID;

    sprintf(name, "%04X%04X", vendor, product);
    printf("Command to exec is \"%s\"\n", name);


    sprintf(vendorStr, "%d", vendor);
    sprintf(prodStr, "%d", product);
    printf("Args are %s/%s\n", vendorStr, prodStr);
     
     newPID = fork();
     if(newPID == 0)
     {
        execlp(name, name, vendorStr, prodStr, NULL);
        printf("Failed to exec new driver (err = %d)\n", errno);
    }
     else if(newPID == -1)
     {
        printf("Fork failed to make new process for driver (err = %d)\n", errno);
     }
}



FILE *openFile(UInt16 vendor, UInt16 product)
{
char name[]="12341234.hex";
char path[256];
char *pathp = path;
FILE *hexFile;

    pathp = getenv("HEXPATH");
    if(pathp != nil)
    {
        strncpy(path, pathp, 256);
    }
    else
    {
        pathp = getenv("PATH");
        strncpy(path, pathp, 256);
    }
//    printf("Found path %s\n", path);
    
    sprintf(name, "%04X%04X.HEX", vendor, product);
    printf("File to open is \"%s\"\n", name);
    do{
        hexFile = fopen(name, "r");
        if(hexFile != nil)
        {
            getcwd(path, 255);
            printf("From directory %s\n", path);
            break;
        }
        
        if(pathp == nil)
        {
            break;
        }
        if( chdir(strsep(&pathp, ":")) != 0)
        {
            printf("chdir failed with errno %d\n", errno);
            perror(nil);
            break;
        }

    }while(1);
    
    return(hexFile);
}

#if 0
    Description of Intel hex records.

Position	Description
1		Record Marker: The first character of the line is always a colon (ASCII 0x3A) to identify 
                        the line as an Intel HEX file
2 - 3	Record Length: This field contains the number of data bytes in the register represented 
                        as a 2-digit hexidecimal number. This is the total number of data bytes, 
                        not including the checksum byte nor the first 9 characters of the line.
4 - 7	Address: This field contains the address where the data should be loaded into the chip. 
                        This is a value from 0 to 65,535 represented as a 4-digit hexidecimal value.
8 - 9	Record Type: This field indicates the type of record for this line. The possible values 
                        are: 00=Register contains normal data. 01=End of File. 02=Extended address.
10 - ?	Data Bytes: The following bytes are the actual data that will be burned into the EPROM. The 
                        data is represented as 2-digit hexidecimal values.
Last 2 characters	Checksum: The last two characters of the line are a checksum for the line. The 
                        checksum value is calculated by taking the twos complement of the sum of all 
                        the preceeding data bytes, excluding the checksum byte itself and the colon 
                        at the beginning of the line.
#endif

IOReturn hexRead(INTEL_HEX_RECORD *record, FILE *hexFile)
{	// Read the next hex record from the file into the structure

    // **** Need to impliment checksum checking ****
    
    
char c;
UInt16 i;
int n, c1, check, len;
    c = getc(hexFile);
    
    if(c != ':')
    {
        printf("Line does not start with colon (%d)\n", c);
        return(kIOReturnNotAligned);
    }
    n = fscanf(hexFile, "%2lX%4lX%2lX", &record->Length, &record->Address, &record->Type);
    if(n != 3)
    {
        printf("Could not read line preamble %d\n", c);
        return(kIOReturnNotAligned);
    }
    
    len = record->Length;
    if(len > MAX_INTEL_HEX_RECORD_LENGTH)
    {
        printf("length is more than can fit %d, %d\n", len, MAX_INTEL_HEX_RECORD_LENGTH);
        return(kIOReturnNotAligned);
   }
    for(i = 0; i<len; i++)
    {
        n = fscanf(hexFile, "%2X", &c1);
        if(n != 1)
        {
            if(i != record->Length)
            {
                printf("Line finished at wrong time %d, %ld\n", i, record->Length);
                return(kIOReturnNotAligned);
            }
        }
        record->Data[i] = c1;
    
    }
    n = fscanf(hexFile, "%2X\n", &check);
    if(n != 1)
    {
        printf("Check not found\n");
        return(kIOReturnNotAligned);
    }
    return(kIOReturnSuccess);
}

IOReturn DownloadToAnchorDevice(IOUSBDeviceInterface245 **dev, UInt16 vendor, UInt16 product)
{
    UInt8 	writeVal;
    IOReturn	kr;
FILE *hexFile;
INTEL_HEX_RECORD anchorCode;
    
    hexFile = openFile(vendor, product);
    if(hexFile == nil)
    {
        printf("File open failed\n");
        return(kIOReturnNotOpen);
    }

    // Assert reset
    writeVal = 1;
    kr = AnchorWrite(dev, k8051_USBCS, 1, &writeVal);
    if (kIOReturnSuccess != kr) 
    {
        printf("AnchorWrite reset returned err 0x%x!\n", kr);

//	Don't do this, the calling function does this on error.
//        (*dev)->USBDeviceClose(dev);
//        (*dev)->Release(dev);
        return kr;
    }
    
    
    
    // Download code
    while (1) 
    {
        kr = hexRead(&anchorCode, hexFile);
        if(anchorCode.Type != 0)
        {
            break;
        }
        if(kr == kIOReturnSuccess)
        {
            kr = AnchorWrite(dev, anchorCode.Address, anchorCode.Length, anchorCode.Data);
        }
        if (kIOReturnSuccess != kr) 
        {
            printf("AnchorWrite download %lx returned err 0x%x!\n", anchorCode.Address, kr);
//	Don't do this, the calling function does this on error.
//            (*dev)->USBDeviceClose(dev);
//            (*dev)->Release(dev);
            return kr;
        }
#if NARRATEDOWNLOAD
        printf("%04lx ",anchorCode.Address);
#endif
    }
#if NARRATEDOWNLOAD
    printf("\n");
#endif

    // De-assert reset
    writeVal = 0;
    kr = AnchorWrite(dev, k8051_USBCS, 1, &writeVal);
    if (kIOReturnSuccess != kr) 
    {
        printf("AnchorWrite run returned err 0x%x!\n", kr);
    }
    
    return kr;
}
      

void RawDeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		usbDevice;
    IOCFPlugInInterface 	**plugInInterface=NULL;
    IOUSBDeviceInterface245 	**dev=NULL;
    HRESULT 			res;
    SInt32 			score;
    UInt16			vendor;
    UInt16			product;
    UInt16			release;
    int exclusiveErr = 0;
    
    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        printf("Raw device added.\n");
       
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        kr = IOObjectRelease(usbDevice);				// done with the device object now that I have the plugin
        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }
            
        // I have the device plugin, I need the device interface
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
        IODestroyPlugInInterface(plugInInterface);			// done with this
		
        if (res || !dev)
        {
            printf("couldn't create a device interface (%08x)\n", (int) res);
            continue;
        }
        // technically should check these kr values
        kr = (*dev)->GetDeviceVendor(dev, &vendor);
        kr = (*dev)->GetDeviceProduct(dev, &product);
        kr = (*dev)->GetDeviceReleaseNumber(dev, &release);

        // need to open the device in order to change its state
        do{
            kr = (*dev)->USBDeviceOpen(dev);
            if(kIOReturnExclusiveAccess == kr)
            {
                exclusiveErr++;
                printf("Exclusive access err, sleeping on it %d\n", exclusiveErr);
                sleep(1);
            }
        }while( (kIOReturnExclusiveAccess == kr) && (exclusiveErr < 5) );
        
        if (kIOReturnSuccess != kr)
        {
            printf("unable to open device: %08x\n", kr);
            (void) (*dev)->Release(dev);
            continue;
        }
        kr = ConfigureAnchorDevice(dev);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to configure device: %08x\n", kr);
            (void) (*dev)->USBDeviceClose(dev);
            (void) (*dev)->Release(dev);
            continue;
        }

        kr = DownloadToAnchorDevice(dev, vendor, product);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to download to device: %08x\n", kr);
            (void) (*dev)->USBDeviceClose(dev);
            (void) (*dev)->Release(dev);
            continue;
        }

        kr = (*dev)->USBDeviceClose(dev);
        kr = (*dev)->Release(dev);
    }
}

void RawDeviceRemoved(void *refCon, io_iterator_t iterator)
{
    kern_return_t	kr;
    io_service_t	obj;
    
    while ( (obj = IOIteratorNext(iterator)) )
    {
        printf("Raw device removed.\n");
        kr = IOObjectRelease(obj);
    }
}

void NewDeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		usbDevice;
    IOCFPlugInInterface 	**plugInInterface=NULL;
    IOUSBDeviceInterface245 	**dev=NULL;
    HRESULT 			res;
    SInt32 			score;
    UInt16			vendor;
    UInt16			product;

    
    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        printf("New device added.\n");
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        kr = IOObjectRelease(usbDevice);				// done with the device object now that I have the plugin
        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }
            
        // I have the device plugin, I need the device interface
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
        IODestroyPlugInInterface(plugInInterface);			// done with this
		
        if (res || !dev)
        {
            printf("couldn't create a device interface (%08x)\n", (int) res);
            continue;
        }
        // technically should check these kr values
        kr = (*dev)->GetDeviceVendor(dev, &vendor);
        kr = (*dev)->GetDeviceProduct(dev, &product);

        runDeviceDriver(vendor, product);

    }
}

void NewDeviceRemoved(void *refCon, io_iterator_t iterator)
{
    kern_return_t	kr;
    io_service_t	obj;
    
    while ( (obj = IOIteratorNext(iterator)) )
    {
        printf("New device removed.\n");
        kr = IOObjectRelease(obj);
    }
}

void SignalHandler(int sigraised)
{
    printf("\nInterrupted\n");
   
    // Clean up here
    IONotificationPortDestroy(gNotifyPort);

    if (gRawAddedIter) 
    {
        IOObjectRelease(gRawAddedIter);
        gRawAddedIter = 0;
    }

    if (gRawRemovedIter) 
    {
        IOObjectRelease(gRawRemovedIter);
        gRawRemovedIter = 0;
    }
    
    if (gNewDeviceAddedIter) 
    {
        IOObjectRelease(gNewDeviceAddedIter);
        gNewDeviceAddedIter = 0;
    }

    if (gNewDeviceRemovedIter) 
    {
        IOObjectRelease(gNewDeviceRemovedIter);
        gNewDeviceRemovedIter = 0;
    }

    // exit(0) should not be called from a signal handler.  Use _exit(0) instead
    //
    _exit(0);
}

int main (int argc, const char *argv[])
{
    mach_port_t 		masterPort;
    CFMutableDictionaryRef 	matchingDict;
    CFRunLoopSourceRef		runLoopSource;
    kern_return_t		kr;
    SInt32			usbVendor = 0xdead;//kOurVendorID;
    SInt32			usbProduct = 0xbeef;// kOurProductID;
    SInt32			usbVendor2;
    SInt32			usbProduct2;
    sig_t			oldHandler;

    if(argc < 3)
    {
        printf("Usage: loadez VID PID [PID2|VID2 PID2]\n");
        return(0);
    }

    // pick up command line arguments
    if (argc > 1)
        usbVendor = atoi(argv[1]);
    if (argc > 2)
        usbProduct = atoi(argv[2]);

    usbVendor2 = usbVendor;
    usbProduct2 = usbProduct+1;
    
    if (argc == 4)
    {
        usbVendor2 = usbVendor;
        usbProduct2 = atoi(argv[3]);
    }
    else
    {
        if (argc > 3)
            usbVendor2 = atoi(argv[3]);
        if (argc > 4)
            usbProduct2 = atoi(argv[4]);
    }

    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR)
        printf("Could not establish new signal handler");
        
    // first create a master_port for my task
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return -1;
    }

    printf("\nLooking for devices matching vendor ID=%ld and product ID=%ld\n", usbVendor, usbProduct);

    // Set up the matching criteria for the devices we're interested in
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class IOUSBDevice and its subclasses
    if (!matchingDict)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }
    
    // Add our vendor and product IDs to the matching criteria
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBVendorID), 
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor)); 
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBProductID), 
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct)); 

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    gNotifyPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
    
    // Retain additional references because we use this same dictionary with four calls to 
    // IOServiceAddMatchingNotification, each of which consumes one reference.
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict ); 
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict ); 
    matchingDict = (CFMutableDictionaryRef) CFRetain( matchingDict ); 
    
    // Now set up two notifications, one to be called when a raw device is first matched by I/O Kit, and the other to be
    // called when the device is terminated.
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOFirstMatchNotification,
                                            matchingDict,
                                            RawDeviceAdded,
                                            NULL,
                                            &gRawAddedIter );
                                            
    RawDeviceAdded(NULL, gRawAddedIter);	// Iterate once to get already-present devices and
                                                // arm the notification

    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOTerminatedNotification,
                                            matchingDict,
                                            RawDeviceRemoved,
                                            NULL,
                                            &gRawRemovedIter );
                                            
    RawDeviceRemoved(NULL, gRawRemovedIter);	// Iterate once to arm the notification
    
    // Change the USB product ID in our matching dictionary to the one the device will have once the
    // firmware has been downloaded.
    printf("Downloaded devices should match vendor ID=%ld and product ID=%ld\n", usbVendor2, usbProduct2);
    
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBVendorID), 
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor2)); 
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBProductID), 
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct2)); 

    // Now set up two more notifications, one to be called when a new test device is first matched by I/O Kit, and the other to be
    // called when the device is terminated.
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOFirstMatchNotification,
                                            matchingDict,
                                            NewDeviceAdded,
                                            NULL,
                                            &gNewDeviceAddedIter );
                                            
    NewDeviceAdded(NULL, gNewDeviceAddedIter);	// Iterate once to get already-present devices and
                                                        // arm the notification

    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOTerminatedNotification,
                                            matchingDict,
                                            NewDeviceRemoved,
                                            NULL,
                                            &gNewDeviceRemovedIter );
                                            
    NewDeviceRemoved(NULL, gNewDeviceRemovedIter); 	// Iterate once to arm the notification

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    // Start the run loop. Now we'll receive notifications.
    CFRunLoopRun();
        
    // We should never get here
    return 0;
}
