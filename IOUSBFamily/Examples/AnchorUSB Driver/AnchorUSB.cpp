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
#include <libkern/OSByteOrder.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBInterface.h>

#include "AnchorUSB.h"
#include "hex2c.h"

extern INTEL_HEX_RECORD demotonehex[];

OSDefineMetaClassAndStructors(com_apple_AnchorUSB, IOService)
#define super IOService

bool 	
com_apple_AnchorUSB::init(OSDictionary *propTable)
{
    IOLog("com_apple_AnchorUSB: init\n");
    return (super::init(propTable));
}



IOService* 
com_apple_AnchorUSB::probe(IOService *provider, SInt32 *score)
{
    IOLog("%s(%p)::probe\n", getName(), this);
    return super::probe(provider, score);			// this returns this
}



bool 
com_apple_AnchorUSB::attach(IOService *provider)
{
    // be careful when performing initialization in this method. It can be and
    // usually will be called mutliple 
    // times per instantiation
    IOLog("%s(%p)::attach\n", getName(), this);
    return super::attach(provider);
}


void 
com_apple_AnchorUSB::detach(IOService *provider)
{
    // Like attach, this method may be called multiple times
    IOLog("%s(%p)::detach\n", getName(), this);
    return super::detach(provider);
}



//
// start
// when this method is called, I have been selected as the driver for this device.
// I can still return false to allow a different driver to load
//
bool 
com_apple_AnchorUSB::start(IOService *provider)
{
    IOReturn 				err;
    UInt8 				writeVal;
    int 				i = 0;
    const IOUSBConfigurationDescriptor *cd;

    // Do all the work here, on an IOKit matching thread.

    IOLog("%s(%p)::start!\n", getName(), this);
    fDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fDevice) 
    {
        IOLog("%s(%p)::start - Provider isn't a USB device!!!\n", getName(), this);
        return false;
    }
    // Find the first config/interface
    if (fDevice->GetNumConfigurations() < 1)
    {
        IOLog("%s(%p)::start - no composite configurations\n", getName(), this);
        return false;
    }
    cd = fDevice->GetFullConfigurationDescriptor(0);

    // set the configuration to the first config
    if (!cd)
    {
        IOLog("%s(%p)::start - no config descriptor\n", getName(), this);
        return false;
    }
	
    if (!fDevice->open(this))
    {
        IOLog("%s(%p)::start - unable to open device for configuration\n", getName(), this);
        return false;
    }
    err = fDevice->SetConfiguration(this, cd->bConfigurationValue, true);
    if (err)
    {
        IOLog("%s(%p)::start - unable to set the configuration\n", getName(), this);
        fDevice->close(this);
        return false;
    }
	

    // Assert reset
    writeVal = 1;
    err = AnchorWrite(k8051_USBCS, 1, &writeVal);
    if(kIOReturnSuccess != err) 
    {
        IOLog("%s(%p)::start - AnchorWrite reset returned err 0x%x!\n", getName(), this, err);
        fDevice->close(this);
        return false;
    }

    // Download code
    while(demotonehex[i].Type == 0) 
    {
        err = AnchorWrite(demotonehex[i].Address, demotonehex[i].Length, demotonehex[i].Data);
        if(kIOReturnSuccess != err)
        {
            IOLog("%s(%p)::start - AnchorWrite download %i returned err 0x%x!\n", getName(), this, i, err);
            fDevice->close(this);
            return false;
        }
        i++;
    }

    // De-assert reset
    writeVal = 0;
    err = AnchorWrite(k8051_USBCS, 1, &writeVal);
    if(kIOReturnSuccess != err) 
    {
        IOLog("%s(%p)::start - AnchorWrite run returned err 0x%x!\n", getName(), this, err);
        fDevice->close(this);
        return false;
    }
    // leave the device open for now, to maintain exclusive access
    return true;
}



void 
com_apple_AnchorUSB::stop(IOService *provider)
{
    IOLog("%s(%p)::stop\n", getName(), this);
    super::stop(provider);
}



bool 
com_apple_AnchorUSB::handleOpen(IOService *forClient, IOOptionBits options, void *arg )
{
    IOLog("%s(%p)::handleOpen\n", getName(), this);
    return super::handleOpen(forClient, options, arg);
}



void 
com_apple_AnchorUSB::handleClose(IOService *forClient, IOOptionBits options )
{
    IOLog("%s(%p)::handleClose\n", getName(), this);
    super::handleClose(forClient, options);
}



IOReturn 
com_apple_AnchorUSB::message(UInt32 type, IOService *provider, void *argument)
{
    IOLog("%s(%p)::message\n", getName(), this);
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            if (fDevice->isOpen(this))
            {
                IOLog("%s(%p)::message - service is terminated - closing device\n", getName(), this);
                fDevice->close(this);
            }
	    break;

        case kIOMessageServiceIsSuspended:
        case kIOMessageServiceIsResumed:
        case kIOMessageServiceIsRequestingClose:
        case kIOMessageServiceWasClosed: 
        case kIOMessageServiceBusyStateChange:
        default:
            break;
    }
    
    return kIOReturnSuccess;
    return super::message(type, provider, argument);
}



bool 
com_apple_AnchorUSB::terminate(IOOptionBits options)
{
    IOLog("%s(%p)::terminate\n", getName(), this);
    return super::terminate(options);
}


bool 
com_apple_AnchorUSB::finalize(IOOptionBits options)
{
    IOLog("%s(%p)::finalize\n", getName(), this);
    return super::finalize(options);
}




IOReturn
com_apple_AnchorUSB::AnchorWrite(UInt16 anchorAddress, UInt16 count, UInt8 writeBuffer[])
{
    IOUSBDevRequest request;
    
    request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    request.bRequest = 0xa0;
    request.wValue = anchorAddress;
    request.wIndex = 0;
    request.wLength = count;
    request.pData = writeBuffer;

    return fDevice->DeviceRequest(&request);
} 

