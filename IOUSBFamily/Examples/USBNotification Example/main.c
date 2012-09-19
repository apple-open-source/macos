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
* © Copyright 2001-2002 Apple Inc.  All rights reserved.
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
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <mach/mach.h>

#include "hex2c.h"

extern INTEL_HEX_RECORD bulktest[];

//#define USE_ASYNC_IO		// Comment this line out if you want to use sync calls for read/write

#define	k8051_USBCS		0x7f92
#define kOurVendorID		1351
#define kOurProductID		8193
#define kOurProductIDBulkTest	4098
#define kTestMessage		"Bulk I/O Test"

// globals
static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gRawAddedIter;
static io_iterator_t		gRawRemovedIter;
static io_iterator_t		gBulkTestAddedIter;
static io_iterator_t		gBulkTestRemovedIter;
static char			gBuffer[64];

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

IOReturn DownloadToAnchorDevice(IOUSBDeviceInterface245 **dev)
{
    int		i;
    UInt8 	writeVal;
    IOReturn	kr;
    
    // Assert reset
    writeVal = 1;
    kr = AnchorWrite(dev, k8051_USBCS, 1, &writeVal);
    if (kIOReturnSuccess != kr) 
    {
        printf("AnchorWrite reset returned err 0x%x!\n", kr);
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
        return kr;
    }
    
    i = 0;
    // Download code
    while (bulktest[i].Type == 0) 
    {
        kr = AnchorWrite(dev, bulktest[i].Address, bulktest[i].Length, bulktest[i].Data);
        if (kIOReturnSuccess != kr) 
        {
            printf("AnchorWrite download %i returned err 0x%x!\n", i, kr);
            (*dev)->USBDeviceClose(dev);
            (*dev)->Release(dev);
            return kr;
        }
        i++;
    }

    // De-assert reset
    writeVal = 0;
    kr = AnchorWrite(dev, k8051_USBCS, 1, &writeVal);
    if (kIOReturnSuccess != kr) 
    {
        printf("AnchorWrite run returned err 0x%x!\n", kr);
    }
    
    return kr;
}
        
void ReadCompletion(void *refCon, IOReturn result, void *arg0)
{
    IOUSBInterfaceInterface245	**intf = (IOUSBInterfaceInterface245 **) refCon;
    UInt32 			numBytesRead = (UInt32) arg0;
    UInt32			i;
    
    printf("Async bulk read complete.\n");
    if (kIOReturnSuccess != result)
    {
        printf("error from async bulk read (%08x)\n", result);
	(void) (*intf)->USBInterfaceClose(intf);
	(void) (*intf)->Release(intf);
        return;
    }
    
    // The firmware we downloaded echoes the 1's complement of what we wrote, so
    // complement the buffer contents to see if we get the original data
    for (i = 0; i < numBytesRead; i++)
        gBuffer[i] = ~gBuffer[i];
    
    printf("Read \"%s\" (%ld bytes) from bulk endpoint\n", gBuffer, numBytesRead);
}

void WriteCompletion(void *refCon, IOReturn result, void *arg0)
{
    IOUSBInterfaceInterface245	**intf = (IOUSBInterfaceInterface245 **) refCon;
    UInt32 			numBytesWritten = (UInt32) arg0;
    UInt32			numBytesRead;
    
    printf("Async write complete.\n");
    if (kIOReturnSuccess != result)
    {
        printf("error from async bulk write (%08x)\n", result);
	(void) (*intf)->USBInterfaceClose(intf);
	(void) (*intf)->Release(intf);
        return;
    }
    printf("Wrote \"%s\" (%ld bytes) to bulk endpoint\n", kTestMessage, numBytesWritten);

    numBytesRead = sizeof(gBuffer) - 1; // leave one byte at the end for NUL termination
    result = (*intf)->ReadPipeAsync(intf, 9, gBuffer, numBytesRead, ReadCompletion, refCon);
    if (kIOReturnSuccess != result)
    {
        printf("unable to do async bulk read (%08x)\n", result);
        (void) (*intf)->USBInterfaceClose(intf);
        (void) (*intf)->Release(intf);
        return;
    }
    
}

IOReturn FindInterfaces(IOUSBDeviceInterface245 **dev)
{
    IOReturn			kr;
    IOUSBFindInterfaceRequest	request;
    io_iterator_t		iterator;
    io_service_t		usbInterface;
    IOCFPlugInInterface 	**plugInInterface = NULL;
    IOUSBInterfaceInterface245 	**intf = NULL;
    HRESULT 			res;
    SInt32 			score;
    UInt8			intfClass;
    UInt8			intfSubClass;
    UInt8			intfNumEndpoints;
    int				pipeRef;
#ifndef USE_ASYNC_IO
    UInt32			numBytesRead;
    UInt32			i;
#else
    CFRunLoopSourceRef		runLoopSource;
#endif
    
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;
   
    kr = (*dev)->CreateInterfaceIterator(dev, &request, &iterator);
    
    while ( (usbInterface = IOIteratorNext(iterator)) )
    {
        printf("Interface found.\n");
       
        kr = IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        kr = IOObjectRelease(usbInterface);				// done with the usbInterface object now that I have the plugin
        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            break;
        }
            
        // I have the interface plugin. I need the interface interface
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245), (LPVOID) &intf);
        IODestroyPlugInInterface(plugInInterface);			// done with this

        if (res || !intf)
        {
            printf("couldn't create an IOUSBInterfaceInterface245 (%08x)\n", (int) res);
            break;
        }
        
        kr = (*intf)->GetInterfaceClass(intf, &intfClass);
        kr = (*intf)->GetInterfaceSubClass(intf, &intfSubClass);
        
        printf("Interface class %d, subclass %d\n", intfClass, intfSubClass);
        
        // Now open the interface. This will cause the pipes to be instantiated that are 
        // associated with the endpoints defined in the interface descriptor.
        kr = (*intf)->USBInterfaceOpen(intf);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to open interface (%08x)\n", kr);
            (void) (*intf)->Release(intf);
            break;
        }
        
    	kr = (*intf)->GetNumEndpoints(intf, &intfNumEndpoints);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to get number of endpoints (%08x)\n", kr);
            (void) (*intf)->USBInterfaceClose(intf);
            (void) (*intf)->Release(intf);
            break;
        }
        
        printf("Interface has %d endpoints.\n", intfNumEndpoints);

        for (pipeRef = 1; pipeRef <= intfNumEndpoints; pipeRef++)
        {
            IOReturn	kr2;
            UInt8	direction;
            UInt8	number;
            UInt8	transferType;
            UInt16	maxPacketSize;
            UInt8	interval;
            char	*message;
            
            kr2 = (*intf)->GetPipeProperties(intf, pipeRef, &direction, &number, &transferType, &maxPacketSize, &interval);
            if (kIOReturnSuccess != kr)
                printf("unable to get properties of pipe %d (%08x)\n", pipeRef, kr2);
            else {
                printf("pipeRef %d: ", pipeRef);

                switch (direction) {
                    case kUSBOut:
                        message = "out";
                        break;
                    case kUSBIn:
                        message = "in";
                        break;
                    case kUSBNone:
                        message = "none";
                        break;
                    case kUSBAnyDirn:
                        message = "any";
                        break;
                    default:
                        message = "???";
                }
                printf("direction %s, ", message);
                
                switch (transferType) {
                    case kUSBControl:
                        message = "control";
                        break;
                    case kUSBIsoc:
                        message = "isoc";
                        break;
                    case kUSBBulk:
                        message = "bulk";
                        break;
                    case kUSBInterrupt:
                        message = "interrupt";
                        break;
                    case kUSBAnyType:
                        message = "any";
                        break;
                    default:
                        message = "???";
                }
                printf("transfer type %s, maxPacketSize %d\n", message, maxPacketSize);
            }
        }
        
        // We can now address endpoints 1 through intfNumEndpoints. Or, we can also address endpoint 0,
        // the default control endpoint. But it's usually better to use (*usbDevice)->DeviceRequest() instead.
#ifndef USE_ASYNC_IO
        kr = (*intf)->WritePipe(intf, 2, kTestMessage, strlen(kTestMessage));
        if (kIOReturnSuccess != kr)
        {
            printf("unable to do bulk write (%08x)\n", kr);
            (void) (*intf)->USBInterfaceClose(intf);
            (void) (*intf)->Release(intf);
            break;
        }
        
        printf("Wrote \"%s\" (%ld bytes) to bulk endpoint\n", kTestMessage, (UInt32) strlen(kTestMessage));
        
        numBytesRead = sizeof(gBuffer) - 1; // leave one byte at the end for NUL termination
        kr = (*intf)->ReadPipe(intf, 9, gBuffer, &numBytesRead);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to do bulk read (%08x)\n", kr);
            (void) (*intf)->USBInterfaceClose(intf);
            (void) (*intf)->Release(intf);
            break;
        }
        // The firmware we downloaded echoes the 1's complement of what we wrote, so
        // complement the buffer contents to see if we get the original data
        for (i = 0; i < numBytesRead; i++)
            gBuffer[i] = ~gBuffer[i];
    
        printf("Read \"%s\" (%ld bytes) from bulk endpoint\n", gBuffer, numBytesRead);
#else
        // Just like with service matching notifications, we need to create an event source and add it 
        //  to our run loop in order to receive async completion notifications.
        kr = (*intf)->CreateInterfaceAsyncEventSource(intf, &runLoopSource);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to create async event source (%08x)\n", kr);
            (void) (*intf)->USBInterfaceClose(intf);
            (void) (*intf)->Release(intf);
            break;
        }
        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);
        
        printf("Async event source added to run loop.\n");
        
        bzero(gBuffer, sizeof(gBuffer));
        strcpy(gBuffer, kTestMessage);
        kr = (*intf)->WritePipeAsync(intf, 2, gBuffer, strlen(gBuffer), WriteCompletion, (void *) intf);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to do async bulk write (%08x)\n", kr);
            (void) (*intf)->USBInterfaceClose(intf);
            (void) (*intf)->Release(intf);
            break;
        }
#endif
        
        // For this test we just want to use the first interface, so exit the loop.
        break;
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
        if ((vendor != kOurVendorID) || (product != kOurProductID) || (release != 1))
        {
            // We should never get here because the matching criteria we specified above
            // will return just those devices with our vendor and product IDs
            printf("found device i didn't want (vendor = %d, product = %d)\n", vendor, product);
            (void) (*dev)->Release(dev);
            continue;
        }

        // need to open the device in order to change its state
        kr = (*dev)->USBDeviceOpen(dev);
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

        kr = DownloadToAnchorDevice(dev);
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

void BulkTestDeviceAdded(void *refCon, io_iterator_t iterator)
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
    
    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        printf("Bulk test device added.\n");
       
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
        if ((vendor != kOurVendorID) || (product != kOurProductIDBulkTest) || (release != 1))
        {
            // We should never get here because the matching criteria we specified above
            // will return just those devices with our vendor and product IDs
            printf("found device i didn't want (vendor = %d, product = %d)\n", vendor, product);
            (*dev)->Release(dev);
            continue;
        }

        // need to open the device in order to change its state
        kr = (*dev)->USBDeviceOpen(dev);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to open device: %08x\n", kr);
            (*dev)->Release(dev);
            continue;
        }
        kr = ConfigureAnchorDevice(dev);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to configure device: %08x\n", kr);
            (*dev)->USBDeviceClose(dev);
            (*dev)->Release(dev);
            continue;
        }

        kr = FindInterfaces(dev);
        if (kIOReturnSuccess != kr)
        {
            printf("unable to find interfaces on device: %08x\n", kr);
            (*dev)->USBDeviceClose(dev);
            (*dev)->Release(dev);
            continue;
        }
        
#ifndef USE_ASYNC_IO
        kr = (*dev)->USBDeviceClose(dev);
        kr = (*dev)->Release(dev);
#endif
    }
}

void BulkTestDeviceRemoved(void *refCon, io_iterator_t iterator)
{
    kern_return_t	kr;
    io_service_t	obj;
    
    while ( (obj = IOIteratorNext(iterator)) )
    {
        printf("Bulk test device removed.\n");
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
    
    if (gBulkTestAddedIter) 
    {
        IOObjectRelease(gBulkTestAddedIter);
        gBulkTestAddedIter = 0;
    }

    if (gBulkTestRemovedIter) 
    {
        IOObjectRelease(gBulkTestRemovedIter);
        gBulkTestRemovedIter = 0;
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
    SInt32			usbVendor = kOurVendorID;
    SInt32			usbProduct = kOurProductID;
    sig_t			oldHandler;
    
    // pick up command line arguments
    if (argc > 1)
        usbVendor = atoi(argv[1]);
    if (argc > 2)
        usbProduct = atoi(argv[2]);

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

    printf("Looking for devices matching vendor ID=%ld and product ID=%ld\n", usbVendor, usbProduct);

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
    // bulktest firmware has been downloaded.
    usbProduct = kOurProductIDBulkTest;
    
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBProductID), 
            CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct)); 

    // Now set up two more notifications, one to be called when a bulk test device is first matched by I/O Kit, and the other to be
    // called when the device is terminated.
    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOFirstMatchNotification,
                                            matchingDict,
                                            BulkTestDeviceAdded,
                                            NULL,
                                            &gBulkTestAddedIter );
                                            
    BulkTestDeviceAdded(NULL, gBulkTestAddedIter);	// Iterate once to get already-present devices and
                                                        // arm the notification

    kr = IOServiceAddMatchingNotification(  gNotifyPort,
                                            kIOTerminatedNotification,
                                            matchingDict,
                                            BulkTestDeviceRemoved,
                                            NULL,
                                            &gBulkTestRemovedIter );
                                            
    BulkTestDeviceRemoved(NULL, gBulkTestRemovedIter); 	// Iterate once to arm the notification

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    // Start the run loop. Now we'll receive notifications.
    CFRunLoopRun();
        
    // We should never get here
    return 0;
}
