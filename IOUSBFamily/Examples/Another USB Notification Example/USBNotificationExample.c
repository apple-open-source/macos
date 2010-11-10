/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
    File:		USBNotificationExample.c
	
    Description:	This sample demonstrates how to use IOKitLib and IOUSBLib to set up asynchronous
                        callbacks when a USB device is attached to or removed from the system.
                        It also shows how to associate arbitrary data with each device instance.
                
    Copyright:		© Copyright 2001 Apple Computer, Inc. All rights reserved.
	
    Disclaimer:		IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
                        ("Apple") in consideration of your agreement to the following terms, and your
                        use, installation, modification or redistribution of this Apple software
                        constitutes acceptance of these terms.  If you do not agree with these terms,
                        please do not use, install, modify or redistribute this Apple software.

                        In consideration of your agreement to abide by the following terms, and subject
                        to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
                        copyrights in this original Apple software (the "Apple Software"), to use,
                        reproduce, modify and redistribute the Apple Software, with or without
                        modifications, in source and/or binary forms; provided that if you redistribute
                        the Apple Software in its entirety and without modifications, you must retain
                        this notice and the following text and disclaimers in all such redistributions of
                        the Apple Software.  Neither the name, trademarks, service marks or logos of
                        Apple Computer, Inc. may be used to endorse or promote products derived from the
                        Apple Software without specific prior written permission from Apple.  Except as
                        expressly stated in this notice, no other rights or licenses, express or implied,
                        are granted by Apple herein, including but not limited to any patent rights that
                        may be infringed by your derivative works or by other works in which the Apple
                        Software may be incorporated.

                        The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
                        WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
                        WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
                        PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
                        COMBINATION WITH YOUR PRODUCTS.

                        IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
                        CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
                        GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
                        ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
                        OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
                        (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
                        ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
				
*/

//================================================================================================
//   Includes
//================================================================================================
//
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <mach/mach.h>
#include <unistd.h>

//================================================================================================
//   Typedefs and Defines
//================================================================================================
//
#define kMyVendorID		1351
#define kMyProductID		8193

typedef struct MyPrivateData {
    io_object_t					notification;
    IOUSBDeviceInterface245 *	*deviceInterface;
    CFStringRef					deviceName;
    UInt32						locationID;
} MyPrivateData;

//================================================================================================
//   Globals
//================================================================================================
//
static IONotificationPortRef	gNotifyPort;
static io_iterator_t		gAddedIter;
static CFRunLoopRef		gRunLoop;

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
    MyPrivateData	*privateDataRef = (MyPrivateData *) refCon;
    
	printf("Device 0x%08x received message 0x%x.\n", service, messageType);

    if (messageType == kIOMessageServiceIsTerminated)
    {
        printf("Device 0x%08x removed.\n", service);
    
        // Dump our private data to stdout just to see what it looks like.
        //
        CFShow(privateDataRef->deviceName);
        printf("It was at location: 0x%lx\n",privateDataRef->locationID);

        // Free the data we're no longer using now that the device is going away
        //
        CFRelease(privateDataRef->deviceName);
        
        if ( privateDataRef->deviceInterface )
            kr = (*privateDataRef->deviceInterface)->Release (privateDataRef->deviceInterface);
        
        kr = IOObjectRelease(privateDataRef->notification);
        
        free(privateDataRef);
    }
}

//================================================================================================
//
//	DeviceAdded
//
//	This routine is the callback for our IOServiceAddMatchingNotification.  When we get called
//	we will look at all the devices that were added and we will:
//
//	1.  Create some private data to relate to each device (in this case we use the service's name
//	    and the location ID of the device
//	2.  Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for this device,
//	    using the refCon field to store a pointer to our private data.  When we get called with
//	    this interest notification, we can grab the refCon and access our private data.
//
//================================================================================================
//
void DeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t		kr;
    io_service_t		usbDevice;
    IOCFPlugInInterface 	**plugInInterface=NULL;
    SInt32 			score;
    HRESULT 			res;

    while ( (usbDevice = IOIteratorNext(iterator)) )
    {
        io_name_t		deviceName;
        CFStringRef		deviceNameAsCFString;	
        MyPrivateData		*privateDataRef = NULL;
        UInt32			locationID;

        printf("Device 0x%08x added.\n", usbDevice);
        
        // Add some app-specific information about this device.
        // Create a buffer to hold the data.
        
        privateDataRef = malloc(sizeof(MyPrivateData));
        bzero( privateDataRef, sizeof(MyPrivateData));
        
        // In this sample we'll just use the service's name.
        //
        kr = IORegistryEntryGetName(usbDevice, deviceName);
	if (KERN_SUCCESS != kr)
        {
            deviceName[0] = '\0';
        }
        
        deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, kCFStringEncodingASCII);
        
        // Dump our data to stdout just to see what it looks like.
        //
        CFShow(deviceNameAsCFString);
        
        privateDataRef->deviceName = deviceNameAsCFString;

        // Now, get the locationID of this device.  In order to do this, we need to create an IOUSBDeviceInterface for
        // our device.  This will create the necessary connections between our user land application and the kernel object
        // for the USB Device.
        //
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

        if ((kIOReturnSuccess != kr) || !plugInInterface)
        {
            printf("unable to create a plugin (%08x)\n", kr);
            continue;
        }

        // I have the device plugin, I need the device interface
        //
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&privateDataRef->deviceInterface);
        IODestroyPlugInInterface(plugInInterface);			// done with this
		
        if (res || !privateDataRef->deviceInterface)
        {
            printf("couldn't create a device interface (%08x)\n", (int) res);
            continue;
        }

        // Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h
        // In this case, we just want the locationID.
        //
        kr = (*privateDataRef->deviceInterface)->GetLocationID(privateDataRef->deviceInterface, &locationID);
        if (KERN_SUCCESS != kr)
        {
            printf("GetLocationID returned %08x\n", kr);
            continue;
        }
        else
        {
            printf("Location ID: 0x%lx\n", locationID);
            
        }

        privateDataRef->locationID = locationID;

        // Register for an interest notification for this device. Pass the reference to our
        // private data as the refCon for the notification.
        //
        kr = IOServiceAddInterestNotification(	gNotifyPort,			// notifyPort
                                               usbDevice,			// service
                                               kIOGeneralInterest,		// interestType
                                               DeviceNotification,		// callback
                                               privateDataRef,			// refCon
                                               &(privateDataRef->notification)	// notification
                                               );

        if (KERN_SUCCESS != kr)
        {
            printf("IOServiceAddInterestNotification returned 0x%08x\n", kr);
        }

        // Done with this io_service_t
        //
        kr = IOObjectRelease(usbDevice);
    }
}

//================================================================================================
//
//	SignalHandler
//
//	This routine will get called when we interrupt the program (usually with a Ctrl-C from the
//	command line).  We clean up so that we don't leak.
//
//================================================================================================
//
void SignalHandler(int sigraised)
{
    printf("\nInterrupted\n");
   
    // Clean up here
    IONotificationPortDestroy(gNotifyPort);

    if (gAddedIter) 
    {
        IOObjectRelease(gAddedIter);
        gAddedIter = 0;
    }

    // exit(0) should not be called from a signal handler.  Use _exit(0) instead
    //
    _exit(0);
}

//================================================================================================
//	main
//================================================================================================
//
int main (int argc, const char *argv[])
{
    mach_port_t 		masterPort;
    CFMutableDictionaryRef 	matchingDict;
    CFRunLoopSourceRef		runLoopSource;
    CFNumberRef			numberRef;
    kern_return_t		kr;
    long			usbVendor = kMyVendorID;
    long			usbProduct = kMyProductID;
    sig_t			oldHandler;
    
    // pick up command line arguments
    //
    if (argc > 1)
        usbVendor = atoi(argv[1]);
    if (argc > 2)
        usbProduct = atoi(argv[2]);

    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    //
    oldHandler = signal(SIGINT, SignalHandler);
    if (oldHandler == SIG_ERR)
        printf("Could not establish new signal handler");
        
    // first create a master_port for my task
    //
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
    {
        printf("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return -1;
    }

    printf("Looking for devices matching vendor ID=%ld and product ID=%ld\n", usbVendor, usbProduct);

    // Set up the matching criteria for the devices we're interested in.  The matching criteria needs to follow
    // the same rules as kernel drivers:  mainly it needs to follow the USB Common Class Specification, pp. 6-7.
    // See also http://developer.apple.com/qa/qa2001/qa1076.html
    // One exception is that you can use the matching dictionary "as is", i.e. without adding any matching criteria
    // to it and it will match every IOUSBDevice in the system.  IOServiceAddMatchingNotification will consume this
    // dictionary reference, so there is no need to release it later on.
    //
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class
                                                                // IOUSBDevice and its subclasses
    if (!matchingDict)
    {
        printf("Can't create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }
    
    // We are interested in all USB Devices (as opposed to USB interfaces).  The Common Class Specification
    // tells us that we need to specify the idVendor, idProduct, and bcdDevice fields, or, if we're not interested
    // in particular bcdDevices, just the idVendor and idProduct.  Note that if we were trying to match an IOUSBInterface,
    // we would need to set more values in the matching dictionary (e.g. idVendor, idProduct, bInterfaceNumber and
    // bConfigurationValue.
    //

    // Create a CFNumber for the idVendor and set the value in the dictionary
    //
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBVendorID), 
            numberRef);
    CFRelease(numberRef);
    
    // Create a CFNumber for the idProduct and set the value in the dictionary
    //
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct);
    CFDictionarySetValue( 
            matchingDict, 
            CFSTR(kUSBProductID), 
            numberRef);
    CFRelease(numberRef);
    numberRef = 0;

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    //
    gNotifyPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);
    
    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    //
    kr = IOServiceAddMatchingNotification(gNotifyPort,			// notifyPort
                                          kIOFirstMatchNotification,	// notificationType
                                          matchingDict,			// matching
                                          DeviceAdded,			// callback
                                          NULL,				// refCon
                                          &gAddedIter			// notification
                                          );		
    
    // Iterate once to get already-present devices and arm the notification
    //                                    
    DeviceAdded(NULL, gAddedIter);

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    // Start the run loop. Now we'll receive notifications.
    //
    printf("Starting run loop.\n");
    CFRunLoopRun();
        
    // We should never get here
    //
    printf("Unexpectedly back from CFRunLoopRun()!\n");

    return 0;
}


