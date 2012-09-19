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
* © Copyright 2002 Apple Inc.  All rights reserved.
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

    // Some functions to talks to the DevaSys  <http://www.devasys.com> "USB I2C/IO,  Interface Board"
    // These come in 2 flavors DevaFuncName and DevaFuncNameD. The two sets are functionally identical
    // but take either a device (USBDeviceInterface) object (the "D" functions 
    // or an Interface (USBInterfaceInterface) object to work on. 
    //
    // Currently defined functions are:
    // 
    //    IOReturn DevaSetIoPortsConfig(IOUSBDeviceInterface245 **dev, UInt32 portBits);
    //    IOReturn DevaReadIoPorts(IOUSBDeviceInterface245 **dev, UInt32 *portBits);
    //    IOReturn DevaWriteIoPorts(IOUSBDeviceInterface245 **dev, UInt32 portBits, UInt32 mask);
    // 
    // The operate on the 20 programmable IO bits which appear on the IO connector.
    // The port bits are mapped as 0x000CBBAA (with the 4 C bits shifted down 4 bits)
    // 
    // DevaSetIoPortsConfig sets the 20 programmable IO ports to either Input or Output.
    // Bits which are 0 are configured as output, bits which are 1 are configured as
    // Input. (Mneumonic, 0 looks like 'O', 1 looks like "I".)
    // 
    // DevaReadIoPorts reads the state of the IO pins. The state of both inputs and
    // outputs are read. Outputs should reflect the state they were last set to
    // (unless you have an horrendous load on them.)
    // 
    // DevaWriteIoPorts sets the state of any output bits. Only bits which have
    // their corresponding mask bit set are affected, and of course only bits
    // which had previously been configured as output can do anything.
    // 
    // if you set the symbol NARRATEIO to non zero, a message will be printed  for
    // every IO attempt.

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include "deva.h"
#include "printInterpretedError.h"

#define NARRATEIO 0

IOReturn DevaSetIoPortsConfig(IOUSBInterfaceInterface245 **intf, UInt32 portBits)
{
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF0; // SetIoPortsConfig
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 4; // 32bit int
    req.pData = &portBits;
    
    portBits = HostToUSBLong(portBits);
            //   000CBBAA <-port mappings
            // 1 is Input (looks like I, 0 is output, looks like O)
#if NARRATEIO
printf("Doing SetIoPortsConfig with portbits %08lx\n", USBToHostLong(portBits));
#endif

    err = (*intf)->ControlRequest(intf, 0, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 4)
        {
            err = kIOReturnUnderrun;
        }
    }
    return(err);
}

IOReturn DevaReadIoPorts(IOUSBInterfaceInterface245 **intf, UInt32 *portBits)
{
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF1; // ReadIoPorts
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 4; // 32bit int
    req.pData = portBits;
    
    *portBits = -1;
            //   000CBBAA <-port mappings
            // 1 is Input (looks like I, 0 is output, looks like O)
    
#if NARRATEIO
printf("Doing ReadIoPorts with portbits %08lx\n", USBToHostLong(*portBits));
#endif
    err = (*intf)->ControlRequest(intf, 0, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 4)
        {
            err = kIOReturnUnderrun;
        }
        else
        {
            *portBits = USBToHostLong(*portBits);
        }
    }
    return(err);
}


IOReturn DevaWriteIoPorts(IOUSBInterfaceInterface245 **intf, UInt32 portBits, UInt32 mask)
{

struct {
    UInt32 portBits;
    UInt32 maskBits;
    }writeBits;
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF1; // WriteIoPorts
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 8; // 32bit int
    req.pData = &writeBits;
    
    writeBits.portBits = HostToUSBLong(portBits);
    writeBits.maskBits = HostToUSBLong(mask);
                    //     000CBBAA <-port mappings
                    // 1 is Input (looks like I, 0 is output, looks like O)
#if NARRATEIO
printf("Doing WriteIoPorts with portbits %08lx, %08lx\n", USBToHostLong(writeBits.portBits), USBToHostLong(writeBits.maskBits));
#endif
   
    err = (*intf)->ControlRequest(intf, 0, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 8)
        {
            err = kIOReturnUnderrun;
        }
    }
    return(err);
}


IOReturn DevaSetIoPortsConfigD(IOUSBDeviceInterface245 **dev, UInt32 portBits)
{
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF0; // SetIoPortsConfig
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 4; // 32bit int
    req.pData = &portBits;
    
    portBits = HostToUSBLong(portBits);
            //   000CBBAA <-port mappings
            // 1 is Input (looks like I, 0 is output, looks like O)
#if NARRATEIO
printf("Doing SetIoPortsConfigD with portbits %08lx\n", USBToHostLong(portBits));
#endif
    err = (*dev)->DeviceRequest(dev, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 4)
        {
            err = kIOReturnUnderrun;
        }
    }
    return(err);
}

IOReturn DevaReadIoPortsD(IOUSBDeviceInterface245 **dev, UInt32 *portBits)
{
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF1; // ReadIoPorts
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 4; // 32bit int
    req.pData = portBits;
    
    *portBits = -1;
            //   000CBBAA <-port mappings
            // 1 is Input (looks like I, 0 is output, looks like O)
    
#if NARRATEIO
printf("Doing ReadIoPortsD with portbits %08lx\n", USBToHostLong(*portBits));
#endif
    err = (*dev)->DeviceRequest(dev, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 4)
        {
            err = kIOReturnUnderrun;
        }
        else
        {
            *portBits = USBToHostLong(*portBits);
        }
    }
    return(err);
}


IOReturn DevaWriteIoPortsD(IOUSBDeviceInterface245 **dev, UInt32 portBits, UInt32 mask)
{

struct {
    UInt32 portBits;
    UInt32 maskBits;
    }writeBits;
IOUSBDevRequest req;
IOReturn err;

    req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    req.bRequest = 0xF1; // WriteIoPorts
    req.wValue = 0;	// unused
    req.wIndex = 0; // unused
    req.wLength = 8; // 32bit int
    req.pData = &writeBits;
    
    writeBits.portBits = HostToUSBLong(portBits);
    writeBits.maskBits = HostToUSBLong(mask);
                    //     000CBBAA <-port mappings
                    // 1 is Input (looks like I, 0 is output, looks like O)
#if NARRATEIO
printf("Doing WriteIoPortsD with portbits %08lx, %08lx\n", USBToHostLong(writeBits.portBits), USBToHostLong(writeBits.maskBits));
#endif
   
    err = (*dev)->DeviceRequest(dev, &req);
    if (kIOReturnSuccess == err) 
    {
        if(req.wLenDone != 8)
        {
            err = kIOReturnUnderrun;
        }
    }
    return(err);
}



