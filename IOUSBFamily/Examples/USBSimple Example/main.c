/*
 * Copyright © 2003-2005 Apple Inc.  All rights reserved.
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
* © Copyright 2001-2005 Apple Inc.  All rights reserved.
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
#include <stdio.h>

#include <mach/mach.h>

#include <CoreFoundation/CFNumber.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

mach_port_t 	masterPort = 0;				// requires <mach/mach.h>
char			outBuf[8096];
char			inBuf[8096];

void
MyCallBackFunction(void *dummy, IOReturn result, void *arg0)
{
	//  UInt8	inPipeRef = (UInt32)dummy;
    
    printf("MyCallbackfunction: %d, %d, %d\n", (int)dummy, (int)result, (int)arg0);
    CFRunLoopStop(CFRunLoopGetCurrent());
}


void transferData(IOUSBInterfaceInterface245 **intf, UInt8 inPipeRef, UInt8 outPipeRef)
{
    IOReturn			err;
    CFRunLoopSourceRef		cfSource;
    int				i;
    
    err = (*intf)->CreateInterfaceAsyncEventSource(intf, &cfSource);
    if (err)
    {
	printf("transferData: unable to create event source, err = %08x\n", err);
	return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
    for (i=0; i < 12; i++)
	outBuf[i] = 'R';
    err = (*intf)->WritePipeAsync(intf, outPipeRef, outBuf, 12, (IOAsyncCallback1)MyCallBackFunction, (void*)(UInt32)inPipeRef);
    if (err)
    {
	printf("transferData: WritePipeAsyncFailed, err = %08x\n", err);
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
	return;
    }
    printf("transferData: calling CFRunLoopRun\n");
    CFRunLoopRun();
    printf("transferData: returned from  CFRunLoopRun\n");
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), cfSource, kCFRunLoopDefaultMode);
}


void dealWithPipes(IOUSBInterfaceInterface245 **intf, UInt8 numPipes)
{
    int					i;
    IOReturn				err;			
    UInt8				inPipeRef = 0;
    UInt8				outPipeRef = 0;
    UInt8				direction, number, transferType, interval;
    UInt16				maxPacketSize;
    
    // pipes are one based, since zero is the default control pipe
    for (i=1; i <= numPipes; i++)
    {
	err = (*intf)->GetPipeProperties(intf, i, &direction, &number, &transferType, &maxPacketSize, &interval);
	if (err)
	{
	    printf("dealWithPipes: unable to get pipe properties for pipe %d, err = %08x\n", i, err);
	    return;
	}
	if (transferType != kUSBBulk)
	{
	    printf("dealWithPipes: skipping pipe %d because it is not a bulk pipe\n", i);
	    continue;
	}
	if ((direction == kUSBIn) && !inPipeRef)
	{
	    printf("dealWithPipes: grabbing BULK IN pipe index %d, number %d\n",i, number);
	    inPipeRef = i;
	}
	if ((direction == kUSBOut) && !outPipeRef)
	{
	    printf("dealWithPipes: grabbing BULK OUT pipe index %d, number %d\n", i, number);
	    outPipeRef = i;
	}
    }
//    if (inPipeRef && outPipeRef)
//	transferData(intf, inPipeRef, outPipeRef);
}

void dealWithInterface(io_service_t usbInterfaceRef)
{
    IOReturn					err;
    IOCFPlugInInterface 		**iodev;		// requires <IOKit/IOCFPlugIn.h>
    IOUSBInterfaceInterface245 	**intf;
    SInt32						score;
    UInt8						numPipes;


    err = IOCreatePlugInInterfaceForService(usbInterfaceRef, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
    if (err || !iodev)
    {
		printf("dealWithInterface: unable to create plugin. ret = %08x, iodev = %p\n", err, iodev);
		return;
    }
    err = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245), (LPVOID)&intf);
	IODestroyPlugInInterface(iodev);				// done with this
	
    if (err || !intf)
    {
		printf("dealWithInterface: unable to create a device interface. ret = %08x, intf = %p\n", err, intf);
		return;
    }
    err = (*intf)->USBInterfaceOpen(intf);
    if (err)
    {
		printf("dealWithInterface: unable to open interface. ret = %08x\n", err);
		return;
    }
    err = (*intf)->GetNumEndpoints(intf, &numPipes);
    if (err)
    {
		printf("dealWithInterface: unable to get number of endpoints. ret = %08x\n", err);
		(*intf)->USBInterfaceClose(intf);
		(*intf)->Release(intf);
		return;
    }
    
    printf("dealWithInterface: found %d pipes\n", numPipes);
    if (numPipes == 0)
    {
		// try alternate setting 1
		err = (*intf)->SetAlternateInterface(intf, 1);
		if (err)
		{
			printf("dealWithInterface: unable to set alternate interface 1. ret = %08x\n", err);
			(*intf)->USBInterfaceClose(intf);
			(*intf)->Release(intf);
			return;
		}
		err = (*intf)->GetNumEndpoints(intf, &numPipes);
		if (err)
		{
			printf("dealWithInterface: unable to get number of endpoints - alt setting 1. ret = %08x\n", err);
			(*intf)->USBInterfaceClose(intf);
			(*intf)->Release(intf);
			return;
		}
		numPipes = 13;  		// workaround. GetNumEndpoints does not work after SetAlternateInterface
    }
    
    if (numPipes)
	dealWithPipes(intf, numPipes);
	
    err = (*intf)->USBInterfaceClose(intf);
    if (err)
    {
		printf("dealWithInterface: unable to close interface. ret = %08x\n", err);
		return;
    }
    err = (*intf)->Release(intf);
    if (err)
    {
		printf("dealWithInterface: unable to release interface. ret = %08x\n", err);
		return;
    }
}


void dealWithDevice(io_service_t usbDeviceRef)
{
    IOReturn						err;
    IOCFPlugInInterface				**iodev;		// requires <IOKit/IOCFPlugIn.h>
    IOUSBDeviceInterface245			**dev;
    SInt32							score;
    UInt8							numConf;
    IOUSBConfigurationDescriptorPtr	confDesc;
    IOUSBFindInterfaceRequest		interfaceRequest;
    io_iterator_t					iterator;
    io_service_t					usbInterfaceRef;
    
    err = IOCreatePlugInInterfaceForService(usbDeviceRef, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
    if (err || !iodev)
    {
		printf("dealWithDevice: unable to create plugin. ret = %08x, iodev = %p\n", err, iodev);
		return;
    }
    err = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
	IODestroyPlugInInterface(iodev);				// done with this

    if (err || !dev)
    {
		printf("dealWithDevice: unable to create a device interface. ret = %08x, dev = %p\n", err, dev);
		return;
    }
    err = (*dev)->USBDeviceOpen(dev);
    if (err)
    {
		printf("dealWithDevice: unable to open device. ret = %08x\n", err);
		return;
    }
    err = (*dev)->GetNumberOfConfigurations(dev, &numConf);
    if (err || !numConf)
    {
		printf("dealWithDevice: unable to obtain the number of configurations. ret = %08x\n", err);
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return;
    }
    printf("dealWithDevice: found %d configurations\n", numConf);
    err = (*dev)->GetConfigurationDescriptorPtr(dev, 0, &confDesc);			// get the first config desc (index 0)
    if (err)
    {
		printf("dealWithDevice:unable to get config descriptor for index 0\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return;
    }
    err = (*dev)->SetConfiguration(dev, confDesc->bConfigurationValue);
    if (err)
    {
		printf("dealWithDevice: unable to set the configuration\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return;
    }
    
    interfaceRequest.bInterfaceClass = kIOUSBFindInterfaceDontCare;		// requested class
    interfaceRequest.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;		// requested subclass
    interfaceRequest.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;		// requested protocol
    interfaceRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;		// requested alt setting
    
    err = (*dev)->CreateInterfaceIterator(dev, &interfaceRequest, &iterator);
    if (err)
    {
		printf("dealWithDevice: unable to create interface iterator\n");
        (*dev)->USBDeviceClose(dev);
        (*dev)->Release(dev);
		return;
    }
    
    while ( (usbInterfaceRef = IOIteratorNext(iterator)) )
    {
		printf("found interface: %p\n", (void*)usbInterfaceRef);
		dealWithInterface(usbInterfaceRef);
		IOObjectRelease(usbInterfaceRef);				// no longer need this reference
    }
    
    IOObjectRelease(iterator);
    iterator = 0;

    err = (*dev)->USBDeviceClose(dev);
    if (err)
    {
		printf("dealWithDevice: error closing device - %08x\n", err);
		(*dev)->Release(dev);
		return;
    }
    err = (*dev)->Release(dev);
    if (err)
    {
		printf("dealWithDevice: error releasing device - %08x\n", err);
		return;
    }
}



int main (int argc, const char * argv[])
{
    kern_return_t			err;
    CFMutableDictionaryRef 	matchingDictionary = 0;		// requires <IOKit/IOKitLib.h>
    SInt32					idVendor = 1351;
    SInt32					idProduct = 8193;
    CFNumberRef				numberRef;
    io_iterator_t			iterator = 0;
    io_service_t			usbDeviceRef;
    
    err = IOMasterPort(MACH_PORT_NULL, &masterPort);				
    if (err)
    {
        printf("USBSimpleExample: could not create master port, err = %08x\n", err);
        return err;
    }
    matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);	// requires <IOKit/usb/IOUSBLib.h>
    if (!matchingDictionary)
    {
        printf("USBSimpleExample: could not create matching dictionary\n");
        return -1;
    }
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idVendor);
    if (!numberRef)
    {
        printf("USBSimpleExample: could not create CFNumberRef for vendor\n");
        return -1;
    }
    CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBVendorID), numberRef);
    CFRelease(numberRef);
    numberRef = 0;
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &idProduct);
    if (!numberRef)
    {
        printf("USBSimpleExample: could not create CFNumberRef for product\n");
        return -1;
    }
    CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBProductID), numberRef);
    CFRelease(numberRef);
    numberRef = 0;
    
    err = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
    matchingDictionary = 0;			// this was consumed by the above call
    
    while ( (usbDeviceRef = IOIteratorNext(iterator)) )
    {
		printf("Found device %p\n", (void*)usbDeviceRef);
		dealWithDevice(usbDeviceRef);
		IOObjectRelease(usbDeviceRef);			// no longer need this reference
    }
    
    IOObjectRelease(iterator);
    iterator = 0;
    
    mach_port_deallocate(mach_task_self(), masterPort);
    return 0;
}
