/*
Copyright (c) 1997-2003 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50ACPI.cpp
 * This file contains the implementation of device driver for a 16650-family
 * serial device connected as an ACPI device.  This subclass provides only the routines
 * necessary to detect and initialize the hardware, as well as map the
 * device registers and field interrupts.  All other functions are handled
 * by the client-class "com_apple_driver_16X50UARTSync".
 * 
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#include "Apple16X50ACPI.h"
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOPlatformExpert.h>

#define super Apple16X50BusInterface
OSDefineMetaClassAndStructors(com_apple_driver_16X50ACPI, com_apple_driver_16X50BusInterface)

IOService *Apple16X50ACPI::
probe(IOService *provider, SInt32 *score)
{
    Provider = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!Provider) {
        IOLog ("Apple16X50ACPI: Attached to non-IOACPIPlatformDevice provider!  Failing probe()\n");
        return NULL;
    }

    // fail probe if polled-mode kprintf driver is active
    UInt32 debugFlags = 0;
    PE_parse_boot_arg("debug", &debugFlags);
    if (debugFlags & 0x8)
        return NULL;

    if (!super::probe(provider, score)) return NULL;

    char buf[80];

    sprintf(buf, "ACPI Device=%s", Provider->getName());
    setProperty(kLocationKey, buf);
    Location = OSDynamicCast(OSString, getProperty(kLocationKey))->getCStringNoCopy();

    // this will be the TTY base name for all UARTS
    setProperty(kIOTTYBaseNameKey, "builtin-serial");

    // this will (eventually) be displayed in NetworkPrefs
    InterfaceBaseName = "Built-in Serial Port";

    // when multiple serial ports are available, each should have an unique ID (_UID)
    InterfaceInstance = 0;
	Provider->evaluateInteger( gIOACPIUniqueIDKey, &InterfaceInstance );

    sprintf(buf, "Apple16X50ACPI%d", (int)InterfaceInstance);
    setName(buf);

    return this;
}

Apple16X50UARTSync *Apple16X50ACPI::
probeUART(void* refCon, Apple16X50UARTSync *uart, OSDictionary *properties)
{
    char buf[80];

    uart = super::probeUART(refCon, uart, properties);
    if (!uart) return false;

    sprintf(buf, "%s Base=0x%x", Location, (UInt16)Map->getPhysicalAddress());
    uart->setProperty(kLocationKey, buf);

    return uart;
}

bool Apple16X50ACPI::
start(IOService *provider)
{
    DEBUG_IOLog("%s: %s\n", Name, Location);

	Map = provider->mapDeviceMemoryWithIndex(0);
    if (!Map) goto fail;

    /*
     * Lookup IO register base address now to avoid this later,
     * since getPhysicalAddress() needs to take a mutex.
     */
    RegBase = Map->getPhysicalAddress();

    probeUART(0);

    if (!UARTInstance) goto fail;
    if (!super::start(provider)) goto fail;

    IOLog("%s: Identified Serial Port on %s\n", Name, Location);
    startUARTs();
    return true;

fail:
    stop(Provider);
    return false;
}

void Apple16X50ACPI::
stop(IOService *provider)
{
    DEBUG_IOLog("%s: stop(%p)\n", Name, provider);
    super::stop(provider);
    DEBUG_IOLog("%s::stop() releasing UARTs\n", Name);
    for (UInt32 i=0; i<UARTInstance; i++)
        RELEASE(UART[i]);

    // It is possible to disable the device here by evaluating the
    // _DIS method on our provider, though turning it back on again
    // will not be so easy (apply resource settings through _SRS).
}

void Apple16X50ACPI::
free()
{
    DEBUG_IOLog("%s: free()\n", Name);
    RELEASE(Map);
    super::free();
}

UInt8 Apple16X50ACPI::
getReg(UInt32 reg, void *refCon)
{
    return (Provider->ioRead8(RegBase + reg, 0));
}

void Apple16X50ACPI::
setReg(UInt32 reg, UInt8 val, void *refCon)
{
    Provider->ioWrite8(RegBase + reg, val, 0);
}
