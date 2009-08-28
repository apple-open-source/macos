/*
Copyright (c) 1997-2008 Apple Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * ApplePCI16X50Serial.cpp
 * This file contains the implementation of device driver for a 16650-family
 * serial device connected as a PCI device.  This subclass provides only the routines
 * necessary to detect and initialize the hardware, as well as map the
 * device registers and field interrupts.  All other functions are handled
 * by the client-class "com_apple_driver_16X50UARTSync".
 * 
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#include "Apple16X50PCI.h"
#include <IOKit/serial/IOSerialKeys.h>

#define BAR_OFFSET_TO_REFCON(bar, off)	( (void *)( ((bar)&0x07)|((off)&0x18) ) )
#define REFCON_TO_OFFSET(ref)		( (UInt32)( ((UInt32)ref) & 0x18 ) )
#define REFCON_TO_BAR(ref)		( (UInt32)( ((UInt32)ref) & 0x07 ) )

#define super Apple16X50BusInterface
OSDefineMetaClassAndStructors(com_apple_driver_16X50PCI, com_apple_driver_16X50BusInterface)

IOService *Apple16X50PCI::
probe(IOService *provider, SInt32 *score)
{
    Provider = OSDynamicCast(IOPCIDevice, provider);
    if (!Provider) {
        IOLog ("Apple16X50PCI: Attached to non-IOPCIDevice provider!  Failing probe()\n");
        return NULL;
    }

    if (!super::probe(provider, score)) return NULL;

    char buf[80];
    UInt8 dev  = Provider->getDeviceNumber();
    UInt8 func = Provider->getFunctionNumber();
    UInt8 bus  = Provider->getBusNumber();
    
    OSData *propData = OSDynamicCast(OSData, Provider->getProperty("AAPL,slot-name"));
    if (propData && (propData->getLength()) < 16)
        snprintf(buf, sizeof (buf), "PCI %s Bus=%d Dev=%d Func=%d", (char *)(propData->getBytesNoCopy()), bus, dev, func);
    else
        snprintf(buf, sizeof (buf), "PCI Bus=%d Dev=%d Func=%d", bus, dev, func);
    setProperty(kLocationKey, buf);
    Location = (OSDynamicCast(OSString, getProperty(kLocationKey))->getCStringNoCopy());

    setProperty(kIOTTYBaseNameKey, "pci-serial");  // this will be the TTY base name for all UARTS
    InterfaceBaseName="PCI Serial Adapter"; // this will (eventually) be displayed in NetworkPrefs
    
    InterfaceInstance=dev;

    snprintf(buf, sizeof (buf), "Apple16X50PCI%d", (int)InterfaceInstance);
    setName(buf);
    
    // turn off all access except Config space (for now)
    Provider->setMemoryEnable(false);
    Provider->setIOEnable(false);
    Provider->setBusMasterEnable(false);

    return this;
}

Apple16X50UARTSync *Apple16X50PCI::
probeUART(void* refCon, Apple16X50UARTSync *uart, OSDictionary *properties)
{
    char buf[80];

    uart = super::probeUART(refCon, uart, properties);
    if (!uart) return false;
    
    snprintf(buf, sizeof (buf), "%s BAR=%lu Offset=%lu", Location, REFCON_TO_BAR((uintptr_t)refCon), REFCON_TO_OFFSET((uintptr_t)refCon));
    uart->setProperty(kLocationKey, buf);
    return uart;    
}

// This routine examines a base address register (BAR) in the PCI Device's
// Config space.  If the BAR meets all the criteria, then it is mapped
bool Apple16X50PCI::
setupBAR (UInt32 bar, UInt32 maxUARTs)
{
    register UInt32 barsize, implemented;
    UInt32 barbase, addr=kIOPCIConfigBaseAddress0+(bar<<2);
    barbase = Provider->configRead32(addr);	
    Provider->configWrite32(addr, ~barbase);
    implemented = barbase ^ (Provider->configRead32(addr)); // implemented bits are 1, all others 0
    if ((barbase & implemented) == 0)
        goto skip;  // This BAR is not configured
    Provider->configWrite32(addr, barbase);
    // BAR size determined by least-significant implemented bit
    for (barsize=1; barsize; barsize<<=1)
        if (barsize & implemented) break;
    DEBUG_IOLog("%s: BAR[%d]=0x%08x:0x%08x (len=%d bytes)\n", Name,
                (int)bar, (int)barbase, (int)implemented, (int)barsize);
    if ((barbase & 0x01) != 0x01) goto skip; // not an I/O Range
    if (barsize > (maxUARTs*kREG_Size)) goto skip; // range is too big
    if (barsize < (kREG_Size)) goto skip; // range is too small
    Map[bar] = Provider->mapDeviceMemoryWithRegister(addr);
    if (!(Map[bar])) goto skip;
    Len[bar]=barsize;
    return true;
    
   skip:
    RELEASE(Map[bar]);
    Len[bar]=0;
    return false;
}

UInt32 Apple16X50PCI::
scanBARforUARTs (UInt32 bar, UInt32 maxUARTs)
{
    register UInt32 count=0, offset;
    
    if (Map[bar] && Len[bar]) {
	for (offset=0; offset<Len[bar]; offset+=kREG_Size) {
            if (probeUART(BAR_OFFSET_TO_REFCON(bar,offset)))
                count++;
            else break; // look no further
        }
	if (!count) { // no UARTs were found
            RELEASE(Map[bar]);
            Len[bar]=0;
	}
    }
    return count;
}

bool Apple16X50PCI::
start(IOService *provider)
{
    UInt32 bar;
    DEBUG_IOLog("%s: %s ID=%04x:%04x SID=%04x:%04x Class=%02x:%02x:%02x Int=%d\n",
        Name, Location,
        (int)(Provider->configRead16(kIOPCIConfigVendorID)),
        (int)(Provider->configRead16(kIOPCIConfigDeviceID)),
        (int)(Provider->configRead16(kIOPCIConfigSubSystemVendorID)),
        (int)(Provider->configRead16(kIOPCIConfigSubSystemID)),
        (int)(Provider->configRead8(kIOPCIConfigClassCode+2)),
        (int)(Provider->configRead8(kIOPCIConfigClassCode+1)),
        (int)(Provider->configRead8(kIOPCIConfigClassCode+0)),
        (int)(Provider->configRead8(kIOPCIConfigInterruptPin))
    );

    // Setup all relevant BARs in a single pass before enabling IO space
    for (bar=0; bar<MAX_BARS; bar++)
        setupBAR(bar, bar?1:4);
        
    // Examine the first MAX_BARS bars.  We allow up to four
    // uarts concatinated in the first bar, or one uart per bar.
    // This covers both common addressing schemes without too many false hits.
    // XXX - This should be alterable via a property
    Provider->setIOEnable(true);
    for (bar=0; bar<MAX_BARS; bar++) {
        UInt32 count = scanBARforUARTs(bar, bar?1:4);
        if (count != 1) break;
    }

    if (UARTInstance==0) goto fail;
    if (!super::start(provider)) goto fail;
    
    IOLog("%s: Identified %d Serial channels at %s\n", Name, (int)UARTInstance, Location);
    startUARTs();

    return true;

fail:
    Provider->setIOEnable(false);
    return false;
}

void Apple16X50PCI::
stop(IOService *provider)
{
    DEBUG_IOLog("%s: stop(%p)\n", Name, provider);
    super::stop(provider);
    DEBUG_IOLog("%s::stop() releasing UARTs\n", Name);
    for (UInt32 i=0; i<UARTInstance; i++)
        RELEASE(UART[i]);
    Provider->setIOEnable(false);
}

void Apple16X50PCI::
free()
{
    UInt32 i;
    DEBUG_IOLog("%s: free()\n", Name);
    for (i=0; i<MAX_BARS; i++) RELEASE(Map[i]);
    super::free();
}

UInt8 Apple16X50PCI::
getReg(UInt32 reg, void *refCon)
{
    return (Provider->ioRead8(reg+REFCON_TO_OFFSET((uintptr_t)refCon), Map[REFCON_TO_BAR((uintptr_t)refCon)]));
}

void Apple16X50PCI::
setReg(UInt32 reg, UInt8 val, void *refCon)
{
    Provider->ioWrite8(reg+REFCON_TO_OFFSET((uintptr_t)refCon), val, Map[REFCON_TO_BAR((uintptr_t)refCon)]);
}

