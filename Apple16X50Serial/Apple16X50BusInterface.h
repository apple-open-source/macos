/*
Copyright (c) 1997-2006 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Apple16X50BusInterface.h
 * This file contains the declarations of a device driver for a 16650-family
 * serial device connected via an unspecified bus.  This class provides only the
 * abstraction used by one or more Apple16X50UARTSync object(s) to get at the
 * underlying hardware.  The real work is done in bus-specific subclasses.
 * 
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#ifndef _APPLE16X50BUSINTERFACE_H
#define _APPLE16X50BUSINTERFACE_H

#include "Apple16X50Serial.h"
#include <IOKit/IOService.h>	// superclass
#include <IOKit/IOInterruptEventSource.h>

#define MAX_UARTS	((UInt32)4)

#define Apple16X50UARTSync com_apple_driver_16X50UARTSync
class Apple16X50UARTSync;

#define Apple16X50BusInterface com_apple_driver_16X50BusInterface
class Apple16X50BusInterface : public IOService
{
    OSDeclareAbstractStructors(com_apple_driver_16X50BusInterface);

public:
#ifdef REFBUG
    virtual void retain() const;
    virtual void release() const;
#endif
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual void free();
    virtual IOWorkLoop *getWorkLoop(void *refCon);
    virtual UInt8 getReg(UInt32 reg, void *refCon)=0;
    virtual void setReg(UInt32 reg, UInt8 val, void *refCon)=0;
    static void handleInterruptAction(OSObject *target, IOInterruptEventSource *source, int count);
protected:
    virtual Apple16X50UARTSync *probeUART(void* refCon, Apple16X50UARTSync *uart=0, OSDictionary *properties=0);
    virtual void startUARTs(bool noSuffix=false);
    virtual void handleInterrupt(IOInterruptEventSource *source, int count);
    virtual void setName(const char *name, const IORegistryPlane *plane=0);

    Apple16X50UARTSync		*UART[MAX_UARTS];
    IOInterruptEventSource	*InterruptSource;
    IOWorkLoop			*WorkLoop;
    UInt32			InterfaceInstance;
    const char			*InterfaceBaseName;
    UInt32			UARTInstance;
    const char			*Name;
};

#include "Apple16X50UARTSync.h"	// client class

#endif /* !_APPLE16X50BUSINTERFACE_H */

