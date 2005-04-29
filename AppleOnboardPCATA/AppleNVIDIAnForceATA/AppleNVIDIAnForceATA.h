/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLENVIDIANFORCEATA_H
#define _APPLENVIDIANFORCEATA_H

#include <IOKit/IOFilterInterruptEventSource.h>
#include "AppleOnboardPCATA.h"
#include "AppleNVIDIAnForceATATiming.h"

struct BusTimings {
    UInt8                   pioModeNumber;
    const TimingParameter * pioTiming;
    UInt8                   dmaModeNumber;
    const TimingParameter * dmaTiming;
    UInt8                   ultraModeNumber;
    bool                    ultraEnabled;
};

struct HardwareInfo {
    UInt32       pciDeviceID;
    UInt8        maxUltraMode;
    const char * deviceName;
};

class AppleNVIDIAnForceATA : public AppleOnboardPCATA
{
    OSDeclareDefaultStructors( AppleNVIDIAnForceATA )

protected:
    IOInterruptEventSource *   fInterruptSource;
    IOWorkLoop *               fWorkLoop;
    BusTimings                 fBusTimings[ kMaxDriveCount ];
    bool                       f80PinCablePresent;
    UInt16                     fBMBaseAddr;
    UInt32                     fUltraModeMask;
    const HardwareInfo *       fHWInfo;

    /* Interrupt event source action */

    static void          interruptOccurred(
                               OSObject * owner,
                               IOInterruptEventSource * source,
                               int count );

    /* Interrupt event source filter */

    static bool          interruptFilter(
                               OSObject * owner,
                               IOFilterInterruptEventSource * source );

    virtual bool         getBMBaseAddress(
                               UInt16 * baseAddr );

    virtual void         resetBusTimings( void );

    virtual void         selectTimingParameter(
                               IOATADevConfig * configRequest,
                               UInt32           unitNumber );

    virtual void         programTimingRegisters( void );

    virtual void         initializeHardware( void );

    virtual void         restoreHardwareState( void );

    virtual void         dumpHardwareRegisters( void );

    virtual void         writeTimingIntervalNS(
                               TimingReg reg,
                               UInt32    drive,
                               UInt32    timeNS );

    virtual void         writeTimingRegister(
                               TimingReg reg,
                               UInt32    drive,
                               UInt8     value );

    virtual UInt32       readTimingIntervalNS(
                               TimingReg reg,
                               UInt32    drive );

    virtual UInt8        readTimingRegister(
                               TimingReg reg,
                               UInt32    drive );

public:
    virtual bool         start( IOService * provider );

    virtual void         free( void );

    virtual IOWorkLoop * getWorkLoop( void ) const;

    virtual IOReturn     provideBusInfo(
                               IOATABusInfo * infoOut );

    virtual IOReturn     getConfig(
                               IOATADevConfig * configOut,
                               UInt32           unit );

    virtual IOReturn     selectConfig(
                               IOATADevConfig * config,
                               UInt32           unit );
};

#endif /* !_APPLENVIDIANFORCEATA_H */
