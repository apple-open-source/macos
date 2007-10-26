/*
Copyright (c) 1997-2006 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50BusInterface.cpp
 * This file contains the basic implementation of device driver for a 16650-family
 * serial device connected via an unspecified bus.  This class provides only the
 * abstraction used by one or more Apple16X50UARTSync object(s) to get at the
 * underlying hardware.  The real work is done in bus-specific subclasses.
 * 
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#include "Apple16X50BusInterface.h"
#include <IOKit/serial/IOSerialKeys.h>
#include <libkern/OSAtomic.h>

SInt32 nextInterfaceInstance = 0;

#define super IOService
OSDefineMetaClassAndAbstractStructors(com_apple_driver_16X50BusInterface, IOService);

IOService *Apple16X50BusInterface::
probe(IOService *provider, SInt32 *score)
{
    DEBUG_IOLog("Apple16X50BusInterface::probe() Compiled with DEBUG defined\n");
#ifdef ASSERT
    Debugger("Apple16X50BusInterface::probe() Compiled with ASSERT defined - Entering debugger to set breakpoints");
#endif
    return super::probe(provider, score);
}

void Apple16X50BusInterface::setName(const char *name, const IORegistryPlane *plane)
{
    DEBUG_IOLog("Apple16X50BusInterface::setName(%s)\n", name);
    super::setName(name, plane);
    Name=getName();
}

bool Apple16X50BusInterface::start(IOService *provider)
{
    DEBUG_IOLog("%s::start() WorkLoop=%p InterruptSource=%p\n", Name, WorkLoop, InterruptSource);
    if (!WorkLoop) {
        WorkLoop = IOWorkLoop::workLoop();
        if (!WorkLoop) return false;
        InterruptSource = IOInterruptEventSource::interruptEventSource(
            this, (IOInterruptEventAction)&handleInterruptAction, getProvider()
        );
        DEBUG_IOLog("%s::start() WorkLoop=%p InterruptSource=%p\n", Name, WorkLoop, InterruptSource);
        if (!InterruptSource) {
            RELEASE(WorkLoop);
            return false;
        }
        WorkLoop->addEventSource(InterruptSource);
        InterruptSource->enable();
    }

    return super::start(provider);
}

void Apple16X50BusInterface::startUARTs(bool noSuffix)
{
    unsigned int masterClock=0;

    OSNumber *masterClockObj = OSDynamicCast(OSNumber, getProperty(kMasterClock));
    if (masterClockObj) {
        masterClock = masterClockObj->unsigned32BitValue();
        DEBUG_IOLog("%s::startUARTs(): Master Clock specified as %d Hz\n", Name, masterClock);
    }
    
    if( !(InterfaceBaseName && (*InterfaceBaseName)) )
        InterfaceBaseName="Serial";
    
    for (unsigned int uart=0; uart<UARTInstance; uart++) {
        if (!(UART[uart])) continue;
        UART[uart]->setProperty(kCFBundleIdentifierKey, getProperty(kCFBundleIdentifierKey));
        if (masterClock)
            UART[uart]->setMasterClock(masterClock);
        else {
            masterClock = UART[uart]->determineMasterClock();
            if (!masterClock)
                continue;
            else
                DEBUG_IOLog("%s::startUARTs(): Master Clock determined to be %d Hz\n", Name, masterClock);
        }
        
        char pre[8], buf[80];
        sprintf(pre, "%d%c", (int)InterfaceInstance, (UARTInstance>1)?('a'+uart):('\0') );

        if ( (!noSuffix)  || (uart > 0) )
            UART[uart]->setProperty(kIOTTYSuffixKey, pre);

        UART[uart]->setProperty(kIOTTYBaseNameKey, getProperty(kIOTTYBaseNameKey));
        sprintf(buf, "%s (%s)", InterfaceBaseName, pre);
        UART[uart]->setProperty(kNPProductNameKey, buf);

        if (!(UART[uart]->start(this))) {
            UART[uart]->detach(this);
            RELEASE(UART[uart]);
        }
    }
}

void Apple16X50BusInterface::stop(IOService *provider)
{
    DEBUG_IOLog("%s::stop()\n", Name);
    super::stop(provider);
    if (InterruptSource) {
        DEBUG_IOLog("%s::stop() releasing InterruptSource\n", Name);
        InterruptSource->disable();
        WorkLoop->removeEventSource(InterruptSource);
        RELEASE(InterruptSource);
    }
}

void Apple16X50BusInterface::free()
{
    DEBUG_IOLog("%s::free()\n", Name);

    if (WorkLoop) {
        DEBUG_IOLog("%s::free() releasing WorkLoop\n", Name);
        RELEASE(WorkLoop);
    }

    super::free();
}

Apple16X50UARTSync * Apple16X50BusInterface::
probeUART(void* refCon, Apple16X50UARTSync *uart, OSDictionary *properties)
{
    if (UARTInstance >= MAX_UARTS) goto fail;
    
    if (!uart) uart = Apple16X50UARTSync::probeUART(this, refCon);
    if (uart) {
        if (!properties) properties = OSDictionary::withCapacity(6);
        if (properties) {
            if (uart->init(properties, refCon)) {
                uart->attach(this);
                UART[UARTInstance++] = uart;
            } else goto fail;
        } else goto fail;
    }
    return uart;
    
fail :
    RELEASE(uart);
    RELEASE(properties);
    return NULL;
}

IOWorkLoop *Apple16X50BusInterface::getWorkLoop(void *refCon)
{
    // We could return a WorkLoop-per-port given the refCon parameter.
    DEBUG_IOLog("%s::getWorkLoop()=%p\n", Name, WorkLoop);
    return WorkLoop;
}

void Apple16X50BusInterface::
handleInterruptAction(OSObject *target, IOInterruptEventSource *source, int count)
{ ((Apple16X50BusInterface *)target)->handleInterrupt(source, count); }

void Apple16X50BusInterface::handleInterrupt(IOInterruptEventSource *source, int count)
{
    UInt32 i;
    DEBUG_IOLog("I%d+", count);
    for (i=0; i<UARTInstance; i++)
        if (UART[i]) UART[i]->interrupt();
    DEBUG_IOLog("-I\n");
}

#ifdef REFBUG
void Apple16X50BusInterface::retain() const
{
    super::retain();
    DEBUG_IOLog("%s::retain()==%d\n", Name, getRetainCount());
}

void Apple16X50BusInterface::release() const
{
    DEBUG_IOLog("%s::release()==%d\n",Name, getRetainCount());
    super::release();
}
#endif
